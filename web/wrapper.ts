type WindowPtr = number;

module WasmWrapper {
    //type declarations for emscripten module and wrappers
    declare var Module: {
        cwrap: (name: string, returnType: string, params: string[]) => any;
        setValue: (ptr: number, value: number, type: string) => void;
    }
    declare var _malloc: (number) => number;
    declare var _free: (number) => void;

    // bindings to C++ functions
    export class Bindings {
        public static initGL:         (width: number, height: number) => number;
        public static sizeWindow:     (window: WindowPtr, width: number, height: number) => number;
        public static drawScene:      (window: WindowPtr) => void;
        public static refreshShaders: (vtx_src: string, frg_src: string) => void;
        public static pasteText:      (window: WindowPtr, txt: string) => boolean;
        
        public static bind_functions() {
            // initialize exported functions
            WasmWrapper.Bindings.initGL     = Module.cwrap(
                'initGL',       'number', ['number', 'number']);
            WasmWrapper.Bindings.sizeWindow = Module.cwrap(
                'size_window',   'number', ['number', 'number', 'number']);
            WasmWrapper.Bindings.drawScene  = Module.cwrap(
                'draw_scene',   '',       ['number']);
            WasmWrapper.Bindings.refreshShaders = Module.cwrap(
                'refresh_shaders', '', ['string', 'string']);
            WasmWrapper.Bindings.pasteText = Module.cwrap(
                'paste_text', 'number', ['number', 'string']);
        }
    }
    
    export class Program {
        // resize observer
        private resizeObserver;
        private window: WindowPtr = 0;

        constructor(private canvas: HTMLCanvasElement) {
            //initialise the GL context, call the compiled native function
            try {
                this.window = Bindings.initGL(canvas.width, canvas.height);
            } catch (e) {
                if (e === "unwind") {
                    // this is some emscripten fuckery related to set_main_loop;
                    // don't know what's going on
                    this.window = 1; // xxx bad fixme
                    console.error("unwind fuckery");
                } else {
                    console.error(e);
                }
            }
            if (this.window === 0) {
                console.log("Could not initialise GL");
                return;
            }
            // @ts-ignore
            this.resizeObserver = new ResizeObserver(() => {
                let sx = canvas.style.width;
                let sy = canvas.style.height;
                Bindings.sizeWindow(
                    this.window,
                    canvas.clientWidth  * window.devicePixelRatio,
                    canvas.clientHeight * window.devicePixelRatio);
                // this is needed because changing the window size
                // causes the canvas layout to reset:
                canvas.style.width  = sx;
                canvas.style.height = sy;
                this.invalidate();
            });
            this.resizeObserver.observe(canvas);
            //request redraw
            this.invalidate();
            document.addEventListener('keydown', this.keydown.bind(this));
            document.addEventListener('paste', this.clipboard_paste.bind(this));
        }
        
        clipboard_paste(evt: ClipboardEvent) {
            if (evt.clipboardData) {
                let txt = evt.clipboardData.getData('text');
                if (txt !== null && txt !== '' && this.window > 1) {
                    if (Bindings.pasteText(this.window, txt)) {
                        evt.preventDefault();
                    }
                }
            }
        }
        
        async keydown(evt: KeyboardEvent) {
            if (evt.key === 'r' && this.window !== 0) {
                const opts: RequestInit = {cache: 'no-cache'};
                const vtx_resp = await fetch('main.vtx', opts)
                const frg_resp = await fetch('main.frg', opts)
                if (vtx_resp.ok && frg_resp.ok) {
                    const vtx = await vtx_resp.text();
                    const frg = await frg_resp.text();
                    Bindings.refreshShaders(vtx, frg);
                }
            }
        }

        //render the scene
        private render() {
            //call the native draw function
            Bindings.drawScene(this.window);
        }

        public invalidate() {
            window.requestAnimationFrame(this.render.bind(this, this.window));
        }
    }
}