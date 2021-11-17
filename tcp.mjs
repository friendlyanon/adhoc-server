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
  connection.on("data", (chunk) => {
    // TODO implement the adhoc server
  });
}

export const tcp = () => createServer(onConnection);
