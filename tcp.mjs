import { createServer } from "net";
import { users } from "./users.mjs";

/**
 * @param {import("net").Socket} connection
 */
function onConnection(connection) {
  const { remoteAddress } = connection;
  if (users.has(remoteAddress)) {
    console.log("Duplicate connection from %s", remoteAddress);
    connection.end();
    return;
  }

  console.log("New connection from %s", remoteAddress);
  const userState = {};
  users.set(remoteAddress, userState);
  connection.on("data", (chunk) => {
    // TODO implement the adhoc server
  });
}

export const tcp = () => createServer(onConnection);
