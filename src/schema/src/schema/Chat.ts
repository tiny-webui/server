/**
 * The format is based on OpenAI's chat completions but with several simplifications.
 * Namely, all data fields are called data instead of text/image_url/refusal as the type
 * already indicates the type of the data.
 * Options other than image_url is also removed in the image_url type.
 */

type DeveloperMessage = {
    role: 'developer';
    content: Array<{
        type: 'text';
        data: string;
    }>;
};

type UserMessage = {
    role: 'user';
    content: Array<{
        type: 'text' | 'image_url';
        data: string;
    }>;
};

type AssistantMessage = {
    role: 'assistant';
    content: Array<{
        type: 'text' | 'refusal';
        data: string;
    }>;
}

type Message = DeveloperMessage | UserMessage | AssistantMessage;

type ChatHistory = Array<Message>;
