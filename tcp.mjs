import { createServer } from "net";
import { users } from "./users.mjs";

/**
 * @param {import("net").Socket} connection
 */
function onConnection(connection) {
  console.log("New connection from %s", connection.remoteAddress);
  const userState = {};
  users.set(connection.remoteAddress, userState);
  connection.on("data", (chunk) => {
    // TODO implement the adhoc server
  });
}

export const tcp = () => createServer(onConnection);
