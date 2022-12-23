<script lang="ts">
    import type { Demo } from "./demo";
    import type { DisplayApi } from "./displayApi";
    import { WIDTH, HEIGHT } from "./displayApi";

    export let demo: Demo;

    let pixels = new Uint8Array(WIDTH * HEIGHT * 3);

    const api: DisplayApi = {
        rgb: (r: number, g: number, b: number) => (r << 16) | (g << 8) | b,
        set_pixel: (x: number, y: number, p: number) => {},
        get_display_buffer: () => pixels,
        get_render_buffer: () => pixels,
        flip_buffer: (copy: boolean) => {},
        clear_buffers: () => {},
    };

    $: ((d) => d.init(api))(demo);
</script>

<h1>{!demo && "No demo loaded"}</h1>
<canvas width={WIDTH} height={HEIGHT} />
