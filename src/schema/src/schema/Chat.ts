/**
 * The format is based on OpenAI's chat completions but with several simplifications.
 * Namely, all data fields are called data instead of text/image_url/refusal as the type
 * already indicates the type of the data.
 * Options other than image_url is also removed in the image_url type.
 */

type Message = {
    role: 'user' | 'assistant' | 'developer';
    content: Array<{
        type: 'text' | 'image_url' | 'refusal';
        data: string;
    }>;
};

type ChatHistory = Array<Message>;
