// @ts-check
import WebSocket from "ws";

class TUIRequestError extends Error {
    /**
     * @param {number} code 
     * @param {string} message 
     */
    constructor(code, message) {
        super(message);
        this.code = code;
    }
}

class TUIClient {
    #address;
    #port;
    #closeHandler;
    /** @type {WebSocket|undefined} */
    #ws = undefined;
    #closedByUser = false;
    #decoder = new TextDecoder('utf-8');
    #encoder = new TextEncoder();
    #idSeed = 0;
    /** 
     * @type {Map<number, {
     *      timeoutHandle: ReturnType<typeof setTimeout>;
     *      resolve: (value: any) => void;
     *      reject: (error: TUIRequestError) => void;
     * }>}
     */
    #pendingRequests = new Map();
    /** 
     * @typedef {{
     *      timeoutHandle?: ReturnType<typeof setTimeout>;
     *      resolve?: (undefined) => void;
     *      messageQueue: Array<{value:any, end?: boolean}>;
     *      error?: TUIRequestError;
     * }} StreamRequest;
     * @type {Map<number, StreamRequest>} 
     */
    #pendingStreamRequests = new Map();
    /**
     * 
     * @param {string} address 
     * @param {number} port 
     * @param {(error: string) => void} closeHandler
     */
    constructor(address, port, closeHandler) {
        this.#address = address;
        this.#port = port;
        this.#closeHandler = closeHandler;
    }

    async connectAsync() {
        if (this.#ws !== undefined) {
            throw new Error("Already connected");
        }
        this.#ws = new WebSocket(`ws://${this.#address}:${this.#port}`);
        await new Promise((resolve, reject) => {
            this.#ws?.on('open', () => {
                resolve(undefined);
            });
            this.#ws?.on('error', (error) => {
                this.#ws = undefined;
                reject(new Error(error.message));
            });
        });
        this.#ws.on('close', () => {
            this.#ws = undefined;
            if (!this.#closedByUser) {
                this.#closeHandler('Connection closed by server');
            }
        });
        this.#ws.on('message', this.#handleMessage.bind(this));
    }

    close() {
        if (this.#ws === undefined) {
            return;
        }
        this.#closedByUser = true;
        this.#ws.close();
        this.#ws = undefined;
        for (const request of this.#pendingRequests.values()) {
            clearTimeout(request.timeoutHandle);
            request.reject(new TUIRequestError(-1, "Connection closed"));
        }
        this.#pendingRequests.clear();
        for (const request of this.#pendingStreamRequests.values()) {
            clearTimeout(request.timeoutHandle);
            request.error = new TUIRequestError(-1, "Connection closed");
            request.resolve?.(undefined);
        }
        this.#pendingStreamRequests.clear();
    }
    
    /**
     * 
     * @param {string} method 
     * @param {any} params
     * @param {number} [timeoutMs]
     * 
     * @returns {Promise<any>} 
     * @throws {TUIRequestError}
     */
    async makeRequestAsync(method, params, timeoutMs = 30000) {
        const id = this.#sendMessage(method, params);
        return new Promise((resolve, reject) => {
            const timeoutHandle = setTimeout(() => {
                this.#pendingRequests.delete(id);
                reject(new TUIRequestError(-1, "Request timeout"));
            }, timeoutMs);
            this.#pendingRequests.set(id, {
                timeoutHandle: timeoutHandle,
                resolve: resolve,
                reject: reject
            });
        });
    }

    /**
     * 
     * @param {string} method 
     * @param {any} params 
     * @param {number} [timeoutMs]
     * 
     * @returns {AsyncGenerator<any, any, void>}
     * @throws {TUIRequestError}
     */
    async * makeStreamRequestAsync(method, params, timeoutMs = 30000) {
        const id = this.#sendMessage(method, params);
        /** @type {StreamRequest} */
        let request = {
            messageQueue: []
        };
        this.#pendingStreamRequests.set(id, request);
        while(true) {
            request.timeoutHandle = setTimeout(() => {
                this.#pendingStreamRequests.delete(id);
                request.error = new TUIRequestError(-1, "Request timeout");
                request.resolve?.(undefined);
            }, timeoutMs);
            await new Promise((resolve) => {
                request.resolve = resolve;
            });

            let response = undefined;
            while ((response = request.messageQueue.shift()) !== undefined) {
                if (response.end){
                    return response.value;
                } else {
                    yield response.value;
                }
            }
            if (request.error !== undefined) {
                throw request.error;
            }
        }
    }

    /**
     * 
     * @param {string} method 
     * @param {any} params 
     */
    sendNotification(method, params) {
        this.#sendMessage(method, params);
    }

    /**
     * 
     * @param {string} method 
     * @param {any} params
     * 
     * @returns {number} 
     */
    #sendMessage(method, params) {
        if (this.#ws === undefined) {
            throw new Error("Not connected");
        }
        const id = this.#idSeed++;
        if (this.#idSeed === Number.MAX_SAFE_INTEGER) {
            this.#idSeed = 0;
        }
        const message = JSON.stringify({
            id: id,
            method: method,
            params: params
        });
        const data = this.#encoder.encode(message);
        this.#ws.send(data);
        return id;
    }

    /**
     * 
     * @param {WebSocket.RawData} data
     * @param {boolean} isBinary 
     */
    #handleMessage(data, isBinary) {
        if (!isBinary) {
            /** There should only be binary data */
            return;
        }
        if (Array.isArray(data)) {
            data = Buffer.concat(data);
        }
        const text = this.#decoder.decode(new Uint8Array(data));
        /**
         * @type {{
         *      id: number;
         *      result: any; 
         * }|{
         *      id: number;
         *      result: any;
         *      end: boolean;
         * }|{
         *      id: number;
         *      error:{
         *          code: number;
         *          message: string;
         *      };
         * }}
         */
        const message = JSON.parse(text);
        if (!('id' in message) || !(Number.isInteger(message.id))){
            return;
        }
        const id = message.id;
        {
            const request = this.#pendingRequests.get(id);
            if (request !== undefined) {
                this.#pendingRequests.delete(id);
                clearTimeout(request.timeoutHandle);
                if ('error' in message) {
                    request.reject(new TUIRequestError(
                        message.error.code, message.error.message));
                } else if ('result' in message) {
                    request.resolve(message.result);
                } else {
                    request.reject(new TUIRequestError(
                        -1, "Invalid response"));
                }
                return;
            }
        }
        {
            const request = this.#pendingStreamRequests.get(id);
            if (request !== undefined) {
                clearTimeout(request.timeoutHandle);
                if ('error' in message) {
                    this.#pendingStreamRequests.delete(id);
                    request.error = new TUIRequestError(message.error.code, message.error.message);
                    request.resolve?.(undefined);
                } else if ('result' in message) {
                    if ('end' in message && message.end === true) {
                        this.#pendingStreamRequests.delete(id);
                        request.messageQueue.push({value: message.result, end: true});
                        request.resolve?.(undefined);
                    } else {
                        request.messageQueue.push({value: message.result, end: false});
                        request.resolve?.(undefined);
                    }
                } else {
                    this.#pendingStreamRequests.delete(id);
                    request.error = new TUIRequestError(
                        -1, "Invalid response");
                    request.resolve?.(undefined);
                }
                return;
            }
        }
        console.log(`No pending request for id ${id}, ignoring message.`);
    }
};

export { TUIClient, TUIRequestError };
