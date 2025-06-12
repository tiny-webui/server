// @ts-check

import {
    quicktype,
    InputData,
    JSONSchemaInput,
    FetchingJSONSchemaStore
} from 'quicktype-core';
import {
    schemaForTypeScriptSources
} from 'quicktype-typescript-input';
import fs from 'fs';
import { argv } from 'process';

async function generateCppFromTypeScript(sourceFile) {
    const schemaInput = new JSONSchemaInput(new FetchingJSONSchemaStore());
    await schemaInput.addSource(schemaForTypeScriptSources([sourceFile]));
    const inputData = new InputData();
    inputData.addInput(schemaInput);
    let result = await quicktype({
        inputData,
        lang: 'cpp',
        rendererOptions: {
            "hide-null-optional": true,
            "boost": false,
            "namespace": `TUI::Schema::${sourceFile.split('/').pop().replace(/\.ts$/, '')}`,
        }
    });
    let lines = result.lines;
    const jsonIncludeIndex = lines.findIndex((line) => line.startsWith('#include "json.hpp"'));
    if (jsonIncludeIndex !== -1) {
        lines[jsonIncludeIndex] = '#include <nlohmann/json.hpp>';
    }
    return lines.join('\n');
}

const scriptRoot = new URL('.', import.meta.url).pathname;

if (argv.length < 3) {
    console.error('Usage: node generate.js <file.ts>');
    process.exit(1);
}
const filePath = argv[2];
const fileName = filePath.split('/').pop();
if (fileName === undefined) {
    console.error('Invalid file path provided.');
    process.exit(1);
}
console.log(`Generating C++ code for ${filePath}...`);
const cppCode = await generateCppFromTypeScript(filePath);
const outputFilePath = `${scriptRoot}../${fileName.replace(/\.ts$/, '.h')}`;
fs.writeFileSync(outputFilePath, cppCode);
console.log(`Generated C++ code at ${outputFilePath}`);

