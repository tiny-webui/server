import { TUIClient } from "./TUIClient.js";
import fs from "fs";
import { exit } from "process";
import readline from 'readline';
import { execSync } from 'child_process';

if (process.argv.length < 3) {
    console.error("Usage: node ChatBot.js <config path>");
    process.exit(1);
}

const config = JSON.parse(fs.readFileSync(process.argv[2]).toString());

const client = new TUIClient('127.0.0.1', 12345, (error) => {
    console.error(error);
});

await client.connectAsync();

const models = await client.makeRequestAsync('getModelList', {});
const modelId = models[0]?.id ?? await client.makeRequestAsync('newModel', config);

await client.makeRequestAsync('getChatList', {
    start: 0,
    quantity: 50
});

const chatId = await client.makeRequestAsync('newChat', {});

const lineReader = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

/** @type {string|undefined} */
let parentId = undefined;
/** @type {Array<{type: string, call_id: string, name: string, arguments: string}>} */
let pendingFunctionCalls = [];

const tools = [
    {
        name: 'run_python',
        description: 'Execute Python code and return stdout/stderr output. Use print() to produce output; bare expressions are not displayed. Use this to run calculations, data processing, or any Python code.',
        parameters: {
            type: 'object',
            properties: {
                code: {
                    type: 'string',
                    description: 'The Python code to execute'
                }
            },
            required: ['code']
        }
    }
];

/**
 * @param {string} prompt
 * @returns {Promise<string>}
 */
function askUser(prompt) {
    return new Promise((resolve) => {
        lineReader.question(prompt, (input) => {
            resolve(input.trim().toLowerCase());
        });
    });
}

/**
 * @param {string} code
 * @returns {string}
 */
function runPython(code) {
    try {
        const output = execSync('python3', {
            input: code,
            encoding: 'utf-8',
            timeout: 30000,
            stdio: ['pipe', 'pipe', 'pipe']
        });
        return output;
    } catch (/** @type {any} */ err) {
        let result = '';
        if (err.stdout) result += err.stdout;
        if (err.stderr) result += err.stderr;
        return result || err.message;
    }
}

/**
 * @param {{type: string, call_id: string, name: string, arguments: string}} call
 * @returns {Promise<{call_id: string, type: string, output: Array<{type: string, data: string}>}>}
 */
async function executeFunctionCall(call) {
    if (call.name === 'run_python') {
        let args;
        try {
            args = JSON.parse(call.arguments);
        } catch {
            return {
                call_id: call.call_id,
                type: 'function_call_output',
                output: [{ type: 'text', data: 'Error: Invalid JSON arguments' }]
            };
        }
        const code = args.code ?? '';
        console.log(`\n[Python Code]\n${code}`);
        const answer = await askUser('\nExecute this code? (y/n): ');
        if (answer !== 'y' && answer !== 'yes') {
            console.log('[Execution denied by user]');
            return {
                call_id: call.call_id,
                type: 'function_call_output',
                output: [{ type: 'text', data: 'Error: Execution denied by user' }]
            };
        }
        const output = runPython(code);
        console.log(`[Output]\n${output}`);
        return {
            call_id: call.call_id,
            type: 'function_call_output',
            output: [{ type: 'text', data: output }]
        };
    }

    console.log(`\n[Unknown Function Call] ${call.name}(${call.arguments})`);
    return {
        call_id: call.call_id,
        type: 'function_call_output',
        output: [{ type: 'text', data: `Error: Unknown function "${call.name}"` }]
    };
}

while(true)
{
    /** @type {Array<object>} */
    let newMessages;

    if (pendingFunctionCalls.length > 0) {
        /** Process pending function calls and send results */
        newMessages = await Promise.all(pendingFunctionCalls.map(call => executeFunctionCall(call)));
        pendingFunctionCalls = [];
    } else {
        /** Get user input */
        const userMessage = await new Promise((resolve) => {
            lineReader.question('User: \n', (input) => {
                resolve(input);
            })
        });
        console.log("");
        newMessages = [{
            role: 'user',
            content:[{
                type: 'text',
                data: userMessage
            }]
        }];
    }

    const stream = client.makeStreamRequestAsync('chatCompletion', {
        id: chatId,
        modelId: modelId,
        parent: parentId,
        messages: newMessages,
        tools: tools
    });

    console.log("Assistant: ");
    let result = undefined;
    pendingFunctionCalls = [];
    /** @type {{call_id?: string, name?: string}|undefined} */
    let currentCall = undefined;

    while(!(result = await stream.next()).done){
        const chunk = result.value;
        if (typeof chunk === 'string') {
            process.stdout.write(chunk);
        } else if (chunk && typeof chunk === 'object') {
            if (chunk.event === 'function_call_start') {
                currentCall = chunk.data ?? {};
            } else if (chunk.event === 'function_call_end') {
                const callData = chunk.data ?? {};
                pendingFunctionCalls.push({
                    type: 'function_call',
                    call_id: currentCall?.call_id ?? callData.call_id ?? '',
                    name: currentCall?.name ?? callData.name ?? '',
                    arguments: callData.arguments ?? ''
                });
                currentCall = undefined;
            }
        }
    }
    console.log("\n");

    if (result === undefined) {
        console.error("No result received from chat completion.");
        exit(1);
    }
    const info = result.value;
    const messageIds = info.messageIds;
    parentId = messageIds[messageIds.length - 1];

    if (pendingFunctionCalls.length > 0) {
        console.log(`[${pendingFunctionCalls.length} function call(s) to process]`);
    }
}
