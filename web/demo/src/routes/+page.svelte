<script lang="ts">
  import Display from '$lib/Display.svelte';
  import { Fire } from '$lib/demos/fire';
  import { Snek } from '$lib/demos/snek';
  import { Scroller } from '$lib/demos/scroller';
  import { Blocks } from '$lib/demos/blocks';
  import type { Demo } from '$lib/demo';

  const demos = [Fire, Snek, Scroller, Blocks];

  let demo: Demo | undefined = Fire;

  function setDemo(name: string) {
    demo = demos.find((d) => d.name === name);
  }
</script>

<!-- <h1>Welcome to SvelteKit</h1>
<p>Visit <a href="https://kit.svelte.dev">kit.svelte.dev</a> to read the documentation</p>
 -->
<Display {demo} />

{#each demos as d}<button on:click={() => setDemo(d.name)}>{d.name}</button>{/each}
