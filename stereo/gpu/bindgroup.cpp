#include <stereo/gpu/bindgroup.h>

namespace stereo {

/*************************
 * BindGroup             *
 *************************/

BindGroup::BindGroup(wgpu::BindGroup&& bindgroup):_bindgroup(bindgroup) {}
BindGroup::BindGroup(const BindGroup& other):
    _bindgroup(other._bindgroup)
{
    if (_bindgroup) _bindgroup.reference();
}
BindGroup::BindGroup(BindGroup&& other):
    _bindgroup(other._bindgroup)
{
    other._bindgroup = nullptr;
}

BindGroup::BindGroup(
        wgpu::Device device,
        BindGroupLayout& layout,
        std::initializer_list<wgpu::BindGroupEntry> entries,
        std::string_view label)
{
    wgpu::BindGroupDescriptor bgd;
    bgd.label      = label.data();
    bgd.layout     = layout;
    bgd.entryCount = entries.size();
    bgd.entries    = entries.begin();
    _bindgroup = device.createBindGroup(bgd);
}

BindGroup::~BindGroup() {
    if (_bindgroup) _bindgroup.release();
}

BindGroup& BindGroup::operator=(const BindGroup& other) {
    // we have to make a temporary because the const qualifiers on the cpp API
    // are all fucked up. incref/decref methods should be const, but it's not
    wgpu::BindGroup tmp = other._bindgroup;
    if (tmp) tmp.reference(); // increment BEFORE release
    if (_bindgroup) _bindgroup.release();
    _bindgroup = tmp;
    return *this;
}

BindGroup& BindGroup::operator=(BindGroup&& other) {
    std::swap(other._bindgroup, _bindgroup);
    return *this;
}

BindGroup BindGroup::create(
    wgpu::Device device,
    wgpu::BindGroupDescriptor descriptor)
{
    return BindGroup(std::move(device.createBindGroup(descriptor)));
}


/*************************
 * BindGroupLayout       *
 *************************/

BindGroupLayout::BindGroupLayout(wgpu::BindGroupLayout&& bindgroup):_layout(bindgroup) {}
BindGroupLayout::BindGroupLayout(const BindGroupLayout& other):
    _layout(other._layout)
{
    if (_layout) _layout.reference();
}
BindGroupLayout::BindGroupLayout(BindGroupLayout&& other):
    _layout(other._layout)
{
    other._layout = nullptr;
}

BindGroupLayout::BindGroupLayout(
    wgpu::Device device,
    std::initializer_list<wgpu::BindGroupLayoutEntry> entries,
    std::string_view label)
{
    wgpu::BindGroupLayoutDescriptor bgld;
    bgld.label      = label.data();
    bgld.entryCount = entries.size();
    bgld.entries    = entries.begin();
    _layout = device.createBindGroupLayout(bgld);
}

BindGroupLayout::~BindGroupLayout() {
    if (_layout) _layout.release();
}

BindGroupLayout& BindGroupLayout::operator=(const BindGroupLayout& other) {
    wgpu::BindGroupLayout tmp = other._layout;
    if (tmp) tmp.reference(); // increment BEFORE release
    if (_layout) _layout.release();
    _layout = tmp;
    return *this;
}

BindGroupLayout& BindGroupLayout::operator=(BindGroupLayout&& other) {
    std::swap(other._layout, _layout);
    return *this;
}

}  // namespace stereo
