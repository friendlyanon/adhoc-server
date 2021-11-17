import { createServer } from "http";
import { MessageChannel } from "worker_threads";
import { users } from "./users.mjs";

function asyncStructuredClone(value) {
  const { port1, port2 } = new MessageChannel();
  return new Promise((resolve) => {
    port2.on("message", resolve);
    port2.on("close", () => port2.close);
    port1.postMessage(value);
    port1.close();
  });
}

const asyncCopyUsers = () => asyncStructuredClone(Array.from(users.entries()));

const asyncWrite = (res, data) => new Promise((resolve, reject) => {
  res.write(data, (error) => {
    if (error != null) {
      reject(error);
    } else {
      resolve();
    }
  });
});

/**
 * @param {import("http").IncomingMessage} req
 * @param {import("http").ServerResponse} res
 */
async function onRequest(req, res) {
  const copyOfUsers = await asyncCopyUsers();
  res.setHeader("content-type", "application/json");
  try {
    await asyncWrite(res, "{");
    let delimiter = "";
    for (const [ip, user] of copyOfUsers) {
      await asyncWrite(res, `${delimiter}"${ip}":`);
      await asyncWrite(res, JSON.stringify(user));
      delimiter = ","
    }
    await asyncWrite(res, "}");
  } catch {}
  res.end();
}

export const http = () => createServer(onRequest);
