import { makeGroupJoinPacket, readConnectPacket, readLoginPacket } from "./packets.mjs";
import { connections, copyUsers } from "./users.mjs";
import { asyncWrite } from "./util.mjs";

export const noop = () => {
};

const productCodeRe = /^[A-Z0-9]{9}$/;
const macRe = /^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i;

/**
 * @param userState
 * @param {Buffer} chunk
 * @returns {string|null}
 */
export function login(userState, chunk) {
  const loginPacket = readLoginPacket(chunk);
  if (loginPacket.name.length === 0) {
    return "Invalid name";
  }
  if (!productCodeRe.test(loginPacket.game)) {
    return `Invalid product code ${loginPacket.game}`;
  }
  if (!macRe.test(loginPacket.mac)) {
    return `Invalid MAC ${loginPacket.mac}`;
  }
  userState.name = loginPacket.name;
  userState.game = loginPacket.game;
  userState.mac = loginPacket.mac.toUpperCase();
  return null;
}

const groupCodeRe = /^(?:[a-z]{,4}|[a-z]{4}\d{,4})$/i;

/**
 * @param {import("net").Socket} connection
 * @param userState
 * @param {Buffer} chunk
 * @returns {Promise<string|null>}
 */
export async function connect(connection, userState, chunk) {
  if (userState.group != null) {
    return `User is already in the group ${userState.group}`;
  }

  const connectPacket = readConnectPacket(chunk);
  if (!groupCodeRe.test(connectPacket.group)) {
    return `Invalid group name ${connectPacket.group}`;
  }

  const { group } = connectPacket;
  const users = copyUsers();
  const userEntry =
    /** @type {UserEntry} */ [connection.remoteAddress, userState];
  userState.group = group;
  for (const entry of users) {
    const { 0: ip, 1: peer } = entry;
    if (peer.group === group) {
      await Promise.all([
        asyncWrite(connections.get(ip), makeGroupJoinPacket(userEntry)),
        asyncWrite(connection, makeGroupJoinPacket(entry)),
      ]);
    }
  }

  return null;
}
