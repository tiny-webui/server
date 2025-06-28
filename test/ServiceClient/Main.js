import { TUIClient } from "./TUIClient.js";
import fs from "fs";

if (process.argv.length < 3) {
    console.error("Usage: node Main.js <config path>");
    process.exit(1);
}

const config = JSON.parse(fs.readFileSync(process.argv[2]).toString());

const client = new TUIClient('127.0.0.1', 12345, (error) => {
    console.error(error);
});

await client.connectAsync();

const models = await client.makeRequestAsync('getModelList', {});
const modelId = models[0]?.id ?? await client.makeRequestAsync('newModel', {
    providerName: 'AzureOpenAI',
    providerParams: config
});

console.log(modelId);

await client.makeRequestAsync('getChatList', {
    start: 0,
    quantity: 50
});

const chatId = await client.makeRequestAsync('newChat', {});

console.log(chatId);

const stream = client.makeStreamRequestAsync('chatCompletion', {
    id: chatId,
    modelId: modelId,
    userMessage: {
        role: 'user',
        content:[{
            type: 'text',
            data: 'Tell me a stroy about rockets'
        }]
    }
});

let result = undefined;
while(!(result = await stream.next()).done){
    const chunk = result.value;
    process.stdout.write(chunk);
}
console.log("");
/** The chat info */
if (result!== undefined){
    console.log(JSON.stringify(result.value));
}



