/// <reference types="w3c-web-usb" />
// Interface to connect and communicate with Hub75 display on RPiPico
// Methods
// - connect
// - disconnect
// - sendFrame
// - reboot
// Properties
// - manufacturer
// - product

export class Pico75 {
    manufacturer: string;
    product: string;
    interfaceNumber = 0;
    endpointIn = 0;
    endpointOut = 0;
    device: USBDevice;

    constructor() { }

    // Set up USB connection
    public async connect(): Promise<boolean> {
        let readLoop = () => {
            this.device.transferIn(this.endpointIn, 64).then(result => {
                //this.onReceive(result.data);
                console.log(new TextDecoder('ascii').decode(result.data));
                readLoop();
            }, error => {
                console.error(error);//this.onReceiveError(error);
            });
        };

        const device = await navigator.usb.requestDevice({ filters: [{ vendorId: 0xcafe }] })
        this.device = device;
        await device.open();
        if (!device.configuration) {
            await device.selectConfiguration(1);
        }
        if (!device.configuration) {
            throw ("Failed to get configuration");
        }
        // Find interface of class 255
        const interfaces = device.configuration.interfaces;
        interfaces.forEach(element => {
            element.alternates.forEach(elementalt => {
                if (elementalt.interfaceClass == 0xFF) {
                    this.interfaceNumber = element.interfaceNumber;
                    elementalt.endpoints.forEach(elementendpoint => {
                        if (elementendpoint.direction == "out") {
                            this.endpointOut = elementendpoint.endpointNumber;
                        }
                        if (elementendpoint.direction == "in") {
                            this.endpointIn = elementendpoint.endpointNumber;
                        }
                    })
                }
            })
        })
        await device.claimInterface(this.interfaceNumber);
        await device.selectAlternateInterface(this.interfaceNumber, 0);
        await device.controlTransferOut({
            requestType: "class",
            recipient: "interface",
            request: 0x22,
            value: 0x01,
            index: this.interfaceNumber
        });

        readLoop();

        return true
    }

    // Tear down USB connection.
    public async disconnect(): Promise<void> {
        return this.device.controlTransferOut({
            'requestType': 'class',
            'recipient': 'interface',
            'request': 0x22,
            'value': 0x00,
            'index': this.interfaceNumber
        }).then(() => this.device.close())
    }

    /** Put device in BOOTSEL mode to upload new flash. */
    async reboot() {
        const buf = new Uint8Array(1);
        buf[0] = 0x42; // "B"
        this.device.transferOut(this.endpointOut, buf);
    }

    async sendFrame() {
        const buf = new Uint8Array(4 + 16 * 3); // 16 pixels per run
        buf[0] = 0x50;
        buf[3] = 16;
        for (let y = 0; y < 32; y++) {
            for (let x = 0; x < 128; x += 16) {
                buf[1] = x;
                buf[2] = y;
                for (let i = 0; i < 16; i++) {
                    buf[4 + i * 3] = (x + i) & 0xff;
                    buf[5 + i * 3] = (y * 2) & 0xff;
                    buf[6 + i * 3] = 0x60;
                }
                await this.device.transferOut(this.endpointOut, buf);
            }
        }
    }

    async sendFrameBlob() {
        const buf = new Uint8Array(128 * 32 * 3 + 1);
        buf[0] = 0x50;
        for (let i = 0; i < 128 * 32; i++) {
            buf[1 + i * 3] = i & 0xff;
            buf[2 + i * 3] = 0x42;
            buf[3 + i * 3] = 0x00;
        }
        await this.device.transferOut(this.endpointOut, buf);
    }
}

/**
 * Connect to Raspberry Pi Pico Hub75 USB device
 */

