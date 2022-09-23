<script>
  import svelteLogo from "./assets/svelte.svg";
  import Counter from "./lib/Counter.svelte";
  import { Pico75 } from "./lib/hub75";

  let pico75 = null;
  let connected = false;
  $: connectBtnString = connected ? "Disconnect device" : "Connect device";

  async function toggleDeviceConnect(_e) {
    if (connected) {
      // Disconnect
      pico75 && (await pico75.disconnect());
      pico75 = null;
      connected = false;
    } else {
      pico75 = new Pico75();
      await pico75.connect();
      connected = true;
    }
  }
  async function resetDevice(_e) {
    pico75 && (await pico75.reboot());
  }
  async function testDevice(_e) {
    pico75 && (await pico75.sendFrame());
  }
</script>

<main>
  <div>
    <a href="https://vitejs.dev" target="_blank">
      <img src="/vite.svg" class="logo" alt="Vite Logo" />
    </a>
    <a href="https://svelte.dev" target="_blank">
      <img src={svelteLogo} class="logo svelte" alt="Svelte Logo" />
    </a>
  </div>
  <h1>Vite + Svelte</h1>

  <button id="connectButton" on:click={toggleDeviceConnect}
    >{connectBtnString}</button
  >
  {#if connected}
    <button on:click={resetDevice}>Reset device</button>
  {/if}
  {#if connected}
    <button on:click={testDevice}>Test device</button>
  {/if}

  <div class="card">
    <Counter />
  </div>

  <p>
    Check out <a href="https://github.com/sveltejs/kit#readme" target="_blank"
      >SvelteKit</a
    >, the official Svelte app framework powered by Vite!
  </p>

  <p class="read-the-docs">Click on the Vite and Svelte logos to learn more</p>
</main>

<style>
  .logo {
    height: 6em;
    padding: 1.5em;
    will-change: filter;
  }
  .logo:hover {
    filter: drop-shadow(0 0 2em #646cffaa);
  }
  .logo.svelte:hover {
    filter: drop-shadow(0 0 2em #ff3e00aa);
  }
  .read-the-docs {
    color: #888;
  }
</style>
