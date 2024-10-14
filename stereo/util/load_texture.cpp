#include <cstdio>
#include <cstdlib>
#include <jpeglib.h>

#include <stereo/util/load_texture.h>

namespace stereo {

Texture load_texture(std::string_view filename, MipGeneratorRef mip_generator) {
    wgpu::Device device = mip_generator->get_device();
    Image img = load_jpeg(filename);
    if (not img.data) {
        return {}; // empty texture
    }
    Texture tex {
        device,
        img.size,
        // xxx: this shit is in srgb but we can't make mipmaps that way :|
        // xxx: bgra because we hard code that shit in a lot of places
        img.channels == 4 ? wgpu::TextureFormat::BGRA8Unorm : wgpu::TextureFormat::R8Unorm,
        filename.data()
    };
    tex.submit_write(img.data.get(), img.channels);
    MipTexture mip_tex { mip_generator, tex };
    mip_tex.generate();
    return tex;
}

Image load_jpeg(std::string_view filename) {
    // Error handling struct
    struct jpeg_error_mgr jerr;

    // Decompression struct
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);

    // Open the JPEG file
    FILE* infile = fopen(filename.data(), "rb");
    if (!infile)
    {
        fprintf(stderr, "Cannot open file %s\n", filename.data());
        return {};
    }

    // Initialize the JPEG decompression object
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);

    // Read JPEG header
    if (jpeg_read_header(&cinfo, true) != 1) {
        fprintf(stderr, "Not a valid JPEG file: %s\n", filename.data());
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return {};
    }

    // Start decompression
    jpeg_start_decompress(&cinfo);

    Image img;

    // Retrieve image dimensions and color channels
    int src_channels = cinfo.output_components; // Usually 3 = RGB, or 1 = Grayscale
    img.size     = {cinfo.output_width, cinfo.output_height};
    img.channels = src_channels == 3 ? 4 : src_channels; // Convert RGB to RGBA

    // Calculate the size of the image buffer
    size_t data_size = img.size.x * img.size.y * img.channels;

    // Allocate memory for the pixel data
    img.data = std::make_shared<uint8_t[]>(data_size);
    constexpr size_t rows_per_read = 8; // one jpeg block
    uint8_t* buffer = new uint8_t[img.size.x * src_channels * rows_per_read];
    if (not img.data and buffer) {
        fprintf(stderr, "Failed to allocate memory for JPEG loading.\n");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        if (buffer) delete[] buffer;
        return {};
    }

    size_t src_row_stride = img.size.x * src_channels;
    size_t dst_row_stride = img.size.x * img.channels;
    uint8_t* buffer_rows[rows_per_read] = {0};
    for (size_t i = 0; i < rows_per_read; ++i) {
        uint8_t* base_buffer = (src_channels == 3) ? buffer : img.data.get();
        buffer_rows[i] = base_buffer + i * src_row_stride;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* dst_scanline_start = img.data.get() + cinfo.output_scanline * dst_row_stride;
        size_t n_read = jpeg_read_scanlines(&cinfo, buffer_rows, rows_per_read);
        // if adding an alpha channel, copy the read data to the final buffer
        // also put into BGRA because everyone else wants that and I don't
        // want to disentangle the format all over the place
        if (src_channels == 3) {
            for (size_t i = 0; i < n_read; ++i) {
                uint8_t* src = buffer_rows[i];
                uint8_t* dst = dst_scanline_start + i * dst_row_stride;
                for (size_t j = 0; j < img.size.x; ++j) {
                    // BGRA!
                    dst[j * 4 + 0] = src[j * 3 + 2]; // B
                    dst[j * 4 + 1] = src[j * 3 + 1]; // G
                    dst[j * 4 + 2] = src[j * 3 + 0]; // R
                    dst[j * 4 + 3] = 0xff; // A
                }
            }
        }
    }

    // Finish decompression and release resources
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    delete [] buffer;

    // Return the raw pixel data
    return img;
}

}  // namespace stereo
