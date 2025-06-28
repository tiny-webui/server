import { TUIClient } from "./TUIClient.js";

const client = new TUIClient('127.0.0.1', 12345, (error) => {
    console.error(error);
});

await client.connectAsync();

const modelList = await client.makeRequestAsync('getModelList', {});

console.log(JSON.stringify(modelList));

for (const model of modelList) {
    await client.makeRequestAsync('deleteModel', model.id);
}

const id = await client.makeRequestAsync('newModel', {
    providerName: 'AzureOpenAI',
    providerParams: {
        url: 'https://this.is.not/a/real/url',
        apiKey: 'this is not a real key'
    }
});

console.log(id);

const settings = await client.makeRequestAsync('getModel', id);

console.log(JSON.stringify(settings));

await client.makeRequestAsync('modifyModel',{
    id,
    settings: {
        providerName: 'AzureOpenAI',
        providerParams: {
            url: 'https://this.is.also.not/a/real/url',
            apiKey: 'this is also not a real key'
        }
    }
});
