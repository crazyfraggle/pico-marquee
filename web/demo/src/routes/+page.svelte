<script lang="ts">
    import Display from "$lib/Display.svelte";
    import {Fire} from "$lib/demos/fire";

    const demos= [Fire];



</script>

<h1>Welcome to SvelteKit</h1>
<p>Visit <a href="https://kit.svelte.dev">kit.svelte.dev</a> to read the documentation</p>

<Display demo={Fire} />

{#each demos as demo}<h2>{demo.name}</h2>{/each}