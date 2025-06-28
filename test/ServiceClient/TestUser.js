import { TUIClient } from "./TUIClient.js";

const client = new TUIClient('127.0.0.1', 12345, (error) => {
    console.error(error);
});

await client.connectAsync();

const users = await client.makeRequestAsync('getUserList', {});

console.log(JSON.stringify(users));

const userName = 'test2';

const existingUser = users.find(user => user.userName === userName);
if (existingUser !== undefined) {
    const settings = await client.makeRequestAsync('getUserAdminSettings', existingUser.id);
    console.log(`User ${userName} already exists with ID: ${existingUser.id}`);
    console.log(`Current settings: ${JSON.stringify(settings)}`);
    console.log(`Deleting user ${userName}...`);
    await client.makeRequestAsync('deleteUser', existingUser.id);
}

const userId = await client.makeRequestAsync('newUser', {
    userName: userName,
    adminSettings: {
        role: 'user'
    },
    credential: {
        password: '123456'
    }
});

console.log(`Created user with ID: ${userId}`);

await client.makeRequestAsync('setUserAdminSettings', {
    id: userId,
    adminSettings: {
        role: 'admin'
    }
});

await client.makeRequestAsync('setUserCredential', {
    id: userId,
    credential: {
        password: '654321'
    }
});
