import { TUIClient } from "./TUIClient.js";
import fs from "fs";
import { exit } from "process";
import readline from 'readline';

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

await client.makeRequestAsync('getChatList', {
    start: 0,
    quantity: 50
});

const chatId = await client.makeRequestAsync('newChat', {});

const lineReader = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

let parentId = undefined;
while(true)
{
    const userMessage = await new Promise((resolve) => {
        lineReader.question('User: \n', (input) => {
            resolve(input);
        })
    });
    console.log("")

    const stream = client.makeStreamRequestAsync('chatCompletion', {
        id: chatId,
        modelId: modelId,
        parent: parentId,
        userMessage: {
            role: 'user',
            content:[{
                type: 'text',
                data: userMessage
            }]
        }
    });

    console.log("Assistant: ");
    let result = undefined;
    while(!(result = await stream.next()).done){
        const chunk = result.value;
        process.stdout.write(chunk);
    }
    console.log("\n");
    /** The chat info */
    if (result === undefined) {
        console.error("No result received from chat completion.");
        exit(1);
    }
    const info = result.value;
    parentId = info.assistantMessageId;
}




