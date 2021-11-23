import { createServer } from "http";
import { parse as parseUrl } from "url";
import { copyUsers } from "./users.mjs";
import { asyncWrite } from "./util.mjs";

/**
 * @param {import("http").IncomingMessage} req
 * @param {import("http").ServerResponse} res
 */
async function onRequest(req, res) {
  if (req.method !== "GET") {
    return res.writeHead(405).end();
  }

  if (parseUrl(req.url, true).pathname !== "/") {
    return res.writeHead(404).end();
  }

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
