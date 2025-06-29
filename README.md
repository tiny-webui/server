## Prerequisites
Install the following packages from your package manager:
* libcurl
* libwebsockets
* libsqlite3
* libuuid
* nlohmann-json

For example, on debian, these can be installed:
```bash
sudo apt install libcurl4-openssl-dev libwebsockets-dev libsqlite3-dev  uuid-dev nlohmann-json3-dev
```
Install the following packages from source:
* [libtev-cpp](https://github.com/chemwolf6922/tiny-event-loop-cpp)
```bash
git clone https://github.com/chemwolf6922/tiny-event-loop-cpp tev-cpp
cd tev-cpp
mkdir build
cd build
cmake ..
make
sudo make install
```
* [js-style-co-routine](https://github.com/chemwolf6922/js-style-co-routine)
```bash
git clone https://github.com/chemwolf6922/js-style-co-routine
cd js-style-co-routine
mkdir build
cd build
cmake ..
make
sudo make install
```

## Build and run tests
The main entry is currently empty. To test the basic functions out, you need to build and run the tests.
```bash
cd test
mkdir build
cd build
cmake ..
make -j$(nproc)
```
To run the end to end test:
1. Place your Azure openai credential under `${PROJECT_ROOT}/test/config/AzureOpenAI.json` with the following format:
```Typescript
{
    /** Full url of your deployment, one that ends with api-version=xxx */
    url: string;
    apiKey: string;
}
```
2. Start the test service. The test service will create a default test admin user. And it will create a new database each time you start the service.
```bash
# In the first console, from project root
mkdir -p test/temp
cd test/build
./TestService ../temp/test.db
```
3. Start the test client.
```bash
# In another console, from project root
cd test/ServiceClient
node ChatBot.js ../config/AzureOpenAI.json
```

## API reference
API design is not finalized. If anything feels wrong, let's discuss it.

For now, some references are [IServer.ts](./src/schema/src/types/IServer.ts), [ChatBot.js](./test/ServiceClient/ChatBot.js), [TestUser.js](./test/ServiceClient/TestUser.js), [TestMetadata.js](./test/ServiceClient/TestMetadata.js) and [Service.cpp](./src/application/Service.cpp). 
