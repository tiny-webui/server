type BulkResponse = {
    choices: Array<{
        finish_reason: string;
        index: number;
        message: {
            content: string;
            refusal: boolean|null;
            role: 'assistant'
        };
    }>;
};

type StreamResponse = {
    choices: Array<{
        delta: {
            content?: string;
        }
    }>;
}
