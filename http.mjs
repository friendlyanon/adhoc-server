import { createServer } from "http";
import { asyncCopyUsers } from "./users.mjs";

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
