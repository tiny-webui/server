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
const schemaRoot = `${scriptRoot}schema/`;
const files = fs.readdirSync(schemaRoot).filter(file => file.endsWith('.ts'));

for (const file of files) {
    console.log(`Generating C++ code for ${file}...`);
    const filePath = `${schemaRoot}${file}`;
    const cppCode = await generateCppFromTypeScript(filePath);
    const outputFilePath = `${scriptRoot}../${file.replace(/\.ts$/, '.h')}`;
    fs.writeFileSync(outputFilePath, cppCode);
    console.log(`Generated C++ code at ${outputFilePath}`);
}

