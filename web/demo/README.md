# create-svelte

Everything you need to build a Svelte project, powered by [`create-svelte`](https://github.com/sveltejs/kit/tree/master/packages/create-svelte).

## Creating a project

If you're seeing this, you've probably already done this step. Congrats!

```bash
# create a new project in the current directory
npm create svelte@latest

# create a new project in my-app
npm create svelte@latest my-app
```

## Developing

Select **Morph clock** to preview the experimental clock animation. Changed
digits start 50 ms apart from right to left (seconds units first, hours tens
250 ms later). Each digit consists of four connected cubic Bezier curves.
On every 750 ms morph, each of its 13 points randomly receives one of six
smooth easing profiles, so parts of the outline lead or lag without oscillating.
A point's x and y share the same easing, curve joins stay connected, and closed
outlines stay closed when morphing between closed glyphs. Every point settles
to the exact final shape. Unchanged digits remain still. The Pico firmware uses
the same timing and easing profiles, sampled into integer lookup tables.

Five small fireworks play behind the digits for 5.7 seconds at the start of
every even-numbered minute (for example, 12:02:00 or 12:04:00). The firmware
plays the same effect only at the top of each hour. Rockets rise, burst into
coloured sparks, then fall and fade; the clock remains in front throughout.
Select Morph clock before the boundary to watch it; missed shows are not replayed.

Once you've created a project and installed dependencies with `npm install` (or `pnpm install` or `yarn`), start a development server:

```bash
npm run dev

# or start the server and open the app in a new browser tab
npm run dev -- --open
```

## Building

To create a production version of your app:

```bash
npm run build
```

You can preview the production build with `npm run preview`.

> To deploy your app, you may need to install an [adapter](https://kit.svelte.dev/docs/adapters) for your target environment.
