import { createServer } from "net";
import * as opcodes from "./opcodes.mjs";
import * as operations from "./operations.mjs";
import { createUser, users } from "./users.mjs";

const todoHandler = () => "Not implemented";

const opcodeMap = new Map([
  [opcodes.PING, todoHandler],
  [opcodes.CONNECT, todoHandler],
  [opcodes.DISCONNECT, todoHandler],
  [opcodes.SCAN, todoHandler],
  [opcodes.SCAN_COMPLETE, todoHandler],
  [opcodes.CONNECT_BSSID, todoHandler],
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
  users.set(remoteAddress, userState);
  const disconnectUser = () => {
    users.delete(remoteAddress);
    // TODO
  };

  connection.on("data", (chunk) => {
    if (chunk.length === 0) {
      return;
    }

    const opcode = chunk[0];
    if (!userState.loggedIn) {
      if (opcode !== opcodes.LOGIN) {
        console.log("%s - Invalid opcode %d in waiting state", displayAddress, opcode);
        disconnectUser();
      }
      const error = operations.login(userState, chunk);
      if (error != null) {
        console.log("%s - %s", displayAddress, error);
        disconnectUser();
      }
    }

    const handler = opcodeMap.get(opcode);
    if (handler == null) {
      console.log("%s - Invalid opcode %d in logged in state", displayAddress, opcode);
      disconnectUser();
    }

    const error = handler(userState, chunk);
    if (error != null) {
      console.log("%s - %s", displayAddress, error);
      disconnectUser();
    }
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
