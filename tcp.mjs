import { createServer } from "net";
import * as opcodes from "./opcodes.mjs";
import * as operations from "./operations.mjs";
import { connections, createUser, users } from "./users.mjs";

const todoHandler =
  ((promise) => () => promise)(Promise.resolve("Not implemented"));

const opcodeMap = new Map([
  [opcodes.CONNECT, operations.connect],
  [opcodes.DISCONNECT, operations.disconnect],
  [opcodes.SCAN, operations.scan],
  [opcodes.CHAT, todoHandler],
]);

/**
 * @param {import("net").Socket} connection
 */
function onConnection(connection) {
  const { remoteAddress } = connection;
  if (remoteAddress == null) {
    connection.end();
    return;
  }

  const displayAddress = remoteAddress.padStart(15, " ");
  if (users.has(remoteAddress)) {
    console.log("%s - Duplicate connection", displayAddress);
    connection.end();
    return;
  }

  console.log("%s - New connection", displayAddress);

  const userState = createUser();
  connections.set(remoteAddress, connection);
  users.set(remoteAddress, userState);
  const disconnectUser = (error) => {
    if (error != null) {
      console.error(error);
    }

    users.delete(remoteAddress);
    connections.delete(remoteAddress);
    const promise = userState.group != null
      ? operations.leaveGroup(remoteAddress, userState.game, userState.group)
      : Promise.resolve(null);
    promise.catch(console.error);
  };

  const errorHandler = (error) => {
    if (error != null) {
      console.log("%s - %s", displayAddress, error);
      disconnectUser();
    }
  };

  connection.on("data", (chunk) => {
    if (chunk.length === 0) {
      return;
    }

    const opcode = chunk[0];
    if (opcode === opcodes.PING) {
      return;
    }

    if (!userState.loggedIn) {
      if (opcode !== opcodes.LOGIN) {
        return errorHandler(`Invalid opcode ${opcode} in waiting state`);
      }
      const error = operations.login(userState, chunk);
      if (error != null) {
        return errorHandler(error);
      }
    }

    const handler = opcodeMap.get(opcode);
    if (handler == null) {
      return errorHandler(`Invalid opcode ${opcode} in logged in state`);
    }

    handler(connection, userState, chunk).then(errorHandler, disconnectUser);
  });

  connection.setTimeout(15_000, () => connection.end());

  const goodbye = () => {
    console.log("%s - Goodbye", displayAddress);
    disconnectUser();
  };

  connection.on("error", (error) => {
    if (error.code === "ECONNRESET") {
      return goodbye();
    }

    console.log("%s - Error", displayAddress);
    disconnectUser();
  });

  connection.on("end", goodbye);
}

export const tcp = () => createServer(onConnection);
