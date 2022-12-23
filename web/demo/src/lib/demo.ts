import type { DisplayApi } from "./displayApi";

export interface Demo {
    name: string;
    init: (api: DisplayApi) => void;
    render: () => boolean;
    keyboard: (c: string) => boolean;
}
