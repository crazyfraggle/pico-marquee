import type { DisplayApi } from "../displayApi";
import type { Demo } from "../demo";

const name = "Fire";

let api: DisplayApi;

const init = (apiRef: DisplayApi) => {
api = apiRef
};

const render = (): boolean => {
    
    return false
};

const keyboard = (char: string): boolean => {
    return false
};

export const Fire: Demo = {
    name,
    init,
    render,
    keyboard
}