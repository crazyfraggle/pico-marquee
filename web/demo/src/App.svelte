<script lang="ts">
  import Display from '$lib/Display.svelte';
  import { Fire } from '$lib/demos/fire';
  import { Snek } from '$lib/demos/snek';
  import { Scroller } from '$lib/demos/scroller';
  import { Blocks } from '$lib/demos/blocks';
  import { Matrix } from '$lib/demos/matrix';
  import { Morph } from '$lib/demos/morph';
  import type { Demo } from '$lib/demo';

  const demos = [Fire, Snek, Scroller, Blocks, Matrix, Morph];

  let demo: Demo | undefined = $state(Fire);

  function setDemo(name: string) {
    demo = demos.find((d) => d.name === name);
  }
</script>

<Display {demo} />

{#each demos as d}<button onclick={() => setDemo(d.name)}>{d.name}</button>{/each}
