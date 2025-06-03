import WebSocket from 'ws';

const ws = new WebSocket('ws://127.0.0.1:12345');

ws.on('open', () => {
    console.log('WebSocket connection opened');
    setTimeout(() => {
        console.log('Closing connection');
        ws.close();
    }, 1000);
});

ws.on('error', (error) => {
    console.error(`WebSocket error: ${error.message}`);
});

ws.on('close', () => {
    console.log('WebSocket connection closed');
});

ws.on('message', (data, isBinary) => {
    if (isBinary) {
        const text = (new TextDecoder()).decode(data);
        console.log(`Received binary data: ${text}`);
    } else {
        console.log(`Received text data: ${data}`);
    }
    ws.send((new TextEncoder()).encode('Hello from client!'), { binary: true });
});
