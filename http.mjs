import { createServer } from "http";
import { copyUsers } from "./users.mjs";
import { asyncWrite } from "./util.mjs";

/**
 * @param {import("http").IncomingMessage} req
 * @param {import("http").ServerResponse} res
 */
async function onRequest(req, res) {
  const copyOfUsers = copyUsers();
  res.setHeader("content-type", "application/json");
  try {
    await asyncWrite(res, "{");
    let delimiter = "";
    for (const { 0: ip, 1: user } of copyOfUsers) {
      await asyncWrite(res, `${delimiter}${JSON.stringify(ip)}:`);
      await asyncWrite(res, JSON.stringify(user));
      delimiter = ",";
    }
    await asyncWrite(res, "}");
  } catch {
  }
  res.end();
}

export const http = () => createServer(onRequest);
