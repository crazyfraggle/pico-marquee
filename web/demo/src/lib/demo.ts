export interface Demo {
  name: string;
  init: () => void;
  render: () => boolean;
  keyboard: (c: string) => boolean;
}
