import { TUIClient } from "./TUIClient.js";

const client = new TUIClient('127.0.0.1', 12345, (error) => {
    console.error(error);
});

await client.connectAsync();

/** Create a model */
const models = await client.makeRequestAsync('getModelList', {});
const modelId = models[0]?.id ?? await client.makeRequestAsync('newModel', {
    providerName: 'AzureOpenAI',
    providerParams: {
        url:'https://this.is.not/a/real/url',
        apiKey: 'this-is-not-a-real-api-key',
    }
});

/** Create a chat */
const chats = await client.makeRequestAsync('getChatList', {
    start: 0,
    quantity: 50
});

const chatId = chats.list[0]?.id ?? await client.makeRequestAsync('newChat', {});

/**
 * 
 * @param {Array<string>} path 
 */
async function TestMetadata(path) {
    await client.makeRequestAsync('setMetadata', {
        path: path,
        entries: {
            testKey: 'testValue'
        }
    });

    const globalMetadata = await client.makeRequestAsync('getMetadata', {
        path: path,
        keys: ['testKey']
    });

    if (globalMetadata.testKey !== 'testValue') {
        throw new Error(`${path.join('.')} metadata set failed`);
    }

    await client.makeRequestAsync('deleteMetadata', {
        path: path,
        keys: ['testKey']
    });

    const globalMetadataAfterDelete = await client.makeRequestAsync('getMetadata', {
        path: ['global'],
        keys: ['testKey']
    });

    if (Object.keys(globalMetadataAfterDelete).length !== 0) {
        throw new Error(`${path.join('.')} metadata delete failed`);
    }

    console.log(`${path.join('.')} metadata test passed`);
}

/** Global metadata */
await TestMetadata(['global']);
await TestMetadata(['user']);
await TestMetadata(['userPublic']);
await TestMetadata(['model', modelId]);
await TestMetadata(['chat', chatId]);

client.close();
