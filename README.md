## Prerequisites
Install the following packages from your package manager:
* libcurl
* libwebsockets
* libsqlite3
* libuuid
* nlohmann-json
* libsodium (>=1.0.19)

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
* [libsodium](https://github.com/jedisct1/libsodium)<br>
Distros like debian and ubuntu only have the older version of libsodium available in their package manager. Thus, you may need to install the latest stable libsodium from source to a local location. And build and run tui-server against it.
```bash
# Create a directory to install the library locally.
mkdir -p $YOUR_LOCAL_LIBRARY_PATH
wget https://download.libsodium.org/libsodium/releases/libsodium-1.0.20-stable.tar.gz
tar -xf libsodium-1.0.20-stable.tar.gz
cd libsodium-stable
./configure --prefix="$YOUR_LOCAL_LIBRARY_PATH" --enable-shared
make -j${nproc}
# This will install to the local path. Root is not required if you have write access to it.
make install

# Now configure tui-server to use that library
cd $TUI_SERVER_PROJECT_PATH
echo "$YOUR_LOCAL_LIBRARY_PATH" > local-lib-path

# Each time you activate a shell to build or run tui-server
. set-local-lib-path.sh
```
You may need to update your linker cache after installing these libraries:
```bash
sudo ldconfig
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
npm i
node ChatBot.js ../config/AzureOpenAI.json
```

## API reference
API design is not finalized. If anything feels wrong, let's discuss it.

For now, some references are [IServer.ts](./src/schema/src/types/IServer.ts), [ChatBot.js](./test/ServiceClient/ChatBot.js), [TestUser.js](./test/ServiceClient/TestUser.js), [TestMetadata.js](./test/ServiceClient/TestMetadata.js) and [Service.cpp](./src/application/Service.cpp). 
