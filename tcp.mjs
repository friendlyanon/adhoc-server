import { createServer } from "net";
import { users } from "./users.mjs";

/**
 * @param {import("net").Socket} connection
 */
function onConnection(connection) {
  const { remoteAddress } = connection;
  const displayAddress = remoteAddress.padStart(15, " ");
  if (users.has(remoteAddress)) {
    console.log("%s - Duplicate connection", displayAddress);
    connection.end();
    return;
  }

  console.log("%s - New connection", displayAddress);
  const userState = {};
  users.set(remoteAddress, userState);
  const disconnectUser = () => {
    users.delete(remoteAddress);
    // TODO
  };

  connection.on("data", (chunk) => {
    // TODO implement the adhoc server
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
