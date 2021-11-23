import { createServer } from "net";
import * as opcodes from "./opcodes.mjs";
import * as operations from "./operations.mjs";
import { createUser, users } from "./users.mjs";

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
    } else {
      // TODO
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
