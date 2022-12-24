<script lang="ts">
  import { onMount } from 'svelte';
  import type { Demo } from './demo';
  import { WIDTH, HEIGHT, get_display_buffer, RED, GRN, BLU, flip_buffer } from './pixels';

  export let demo: Demo | undefined;

  let canvas: HTMLCanvasElement;
  let pixelSize = 8;
  $: ((d) => d?.init())(demo);

  onMount(() => {
    let skipFrame = false;
    let rid = requestAnimationFrame(function update() {
      if (demo && !skipFrame) {
        const buf = get_display_buffer();

        // Keep canvas at full width, also blanks canvas
        canvas.width = document.body.clientWidth;
        canvas.height = (canvas.width / WIDTH) * HEIGHT;

        const ctx = canvas.getContext('2d', { alpha: false });

        if (ctx) {
          ctx.setTransform(canvas.width / WIDTH, 0, 0, canvas.height / HEIGHT, 0, 0);
          for (let x = 0; x < WIDTH; x++) {
            for (let y = 0; y < HEIGHT; y++) {
              ctx.fillStyle = `rgb(${buf[RED(x, y)]},${buf[GRN(x, y)]},${buf[BLU(x, y)]})`;
              ctx.fillRect(x, y, 1, 1);
            }
          }
        }
        flip_buffer(true);
        demo.render();
      }
      skipFrame = !skipFrame;
      rid = requestAnimationFrame(update);
    });
    return () => cancelAnimationFrame(rid);
  });
  function onKeyDown(e: KeyboardEvent) {
    // console.log(e);
    if (demo) {
      demo.keyboard(e.key);
    }
  }
</script>

{#if demo}
  <h1>{demo.name}</h1>
  <canvas bind:this={canvas} />
{:else}
  <h1>No demo loaded</h1>
{/if}
<svelte:window on:keydown|preventDefault={onKeyDown} />

<style>
  canvas {
    background-color: black;
  }
</style>
