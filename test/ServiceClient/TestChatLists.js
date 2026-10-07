import assert from 'node:assert/strict';
import { randomUUID } from 'node:crypto';
import { once } from 'node:events';
import { createServer } from 'node:http';
import { TUIClient } from './TUIClient.js';

const provider = createServer((request, response) => {
    request.resume();
    request.on('end', () => {
        response.writeHead(200, { 'Content-Type': 'text/event-stream' });
        response.end(`data: ${JSON.stringify({ choices: [{ delta: { content: 'Saved reply' } }] })}\n\ndata: [DONE]\n\n`);
    });
});
provider.listen(0, '127.0.0.1');
await once(provider, 'listening');

const writer = new TUIClient('127.0.0.1', 12345, console.error);
const reader = new TUIClient('127.0.0.1', 12345, console.error);
const listParams = { start: 0, quantity: 50, metaDataKeys: ['title'] };
const pinParams = { metaDataKeys: ['title'] };
const ids = list => list.map(chat => chat.id);
const hasCode = code => error => error.code === code;

try {
    await writer.connectAsync();
    await reader.connectAsync();
    assert.deepEqual(await writer.makeRequestAsync('getChatList', listParams), []);
    const first = await writer.makeRequestAsync('newChat', {});
    const second = await writer.makeRequestAsync('newChat', {});
    await writer.makeRequestAsync('setMetadata', { path: ['chat', first], entries: { title: 'First' } });
    await writer.makeRequestAsync('setMetadata', { path: ['chat', second], entries: { title: 'Second' } });
    const initial = await writer.makeRequestAsync('getChatList', listParams);
    assert.deepEqual(await reader.makeRequestAsync('getChatList', listParams), initial);
    assert.deepEqual(await writer.makeRequestAsync('getPinnedChatList', pinParams), []);
    assert.deepEqual(await reader.makeRequestAsync('getPinnedChatList', pinParams), []);

    await writer.makeRequestAsync('setChatPinned', { id: first, pinned: true });
    const firstPin = [{ id: first, metadata: { title: 'First' } }];
    assert.deepEqual(await writer.makeRequestAsync('getPinnedChatList', pinParams), firstPin);
    assert.deepEqual(await reader.makeRequestAsync('getPinnedChatList', pinParams), firstPin);
    await assert.rejects(reader.makeRequestAsync('getChatList', listParams), hasCode(304));
    await writer.makeRequestAsync('setChatPinned', { id: second, pinned: true });
    assert.deepEqual(ids(await writer.makeRequestAsync('getPinnedChatList', pinParams)), ids(initial));
    assert.deepEqual(ids(await reader.makeRequestAsync('getPinnedChatList', pinParams)), ids(initial));

    await writer.makeRequestAsync('getModelList', {});
    const modelId = await writer.makeRequestAsync('newModel', {
        providerName: 'AzureOpenAI',
        providerParams: { url: `http://127.0.0.1:${provider.address().port}`, apiKey: 'test-only' },
    });
    const updatedId = initial.at(-1).id;
    const completionParams = {
        id: updatedId,
        modelId,
        messages: [{ role: 'user', content: [{ type: 'text', data: 'Update this chat' }] }],
    };
    let reply = '';
    for await (const segment of writer.makeStreamRequestAsync('chatCompletion', completionParams)) {
        reply += segment;
    }
    assert.equal(reply, 'Saved reply');
    await assert.rejects(reader.makeRequestAsync('getChatList', { ...listParams, start: 1 }), hasCode(409));
    assert.equal((await writer.makeRequestAsync('getChatList', listParams))[0].id, updatedId);
    assert.equal((await reader.makeRequestAsync('getChatList', listParams))[0].id, updatedId);
    assert.equal((await writer.makeRequestAsync('getPinnedChatList', pinParams))[0].id, updatedId);
    assert.equal((await reader.makeRequestAsync('getPinnedChatList', pinParams))[0].id, updatedId);

    await assert.rejects(async () => {
        for await (const segment of writer.makeStreamRequestAsync('chatCompletion', { ...completionParams, parent: randomUUID() })) {
            void segment;
        }
    }, hasCode(404));
    await assert.rejects(reader.makeRequestAsync('getChatList', listParams), hasCode(304));
    await assert.rejects(reader.makeRequestAsync('getPinnedChatList', pinParams), hasCode(304));

    await writer.makeRequestAsync('setMetadata', { path: ['chat', updatedId], entries: { title: 'Renamed' } });
    assert.equal((await writer.makeRequestAsync('getChatList', listParams))[0].id, updatedId);
    assert.equal((await reader.makeRequestAsync('getPinnedChatList', pinParams))[0].metadata.title, 'Renamed');
    await writer.makeRequestAsync('setChatPinned', { id: updatedId, pinned: false });
    assert.equal((await writer.makeRequestAsync('getPinnedChatList', pinParams)).length, 1);
    await writer.makeRequestAsync('deleteChat', updatedId === first ? second : first);
    assert.deepEqual(await reader.makeRequestAsync('getPinnedChatList', pinParams), []);
    await assert.rejects(writer.makeRequestAsync('setChatPinned', { id: randomUUID(), pinned: true }), hasCode(404));
    await assert.rejects(writer.makeRequestAsync('setChatPinned', { id: first, pinned: 'true' }), hasCode(400));
    console.log('Chat list RPC checks passed: pin/unpin, activity order, failures, metadata, deletion, and cross-client invalidation.');
} finally {
    writer.close();
    reader.close();
    await new Promise(resolve => provider.close(resolve));
}