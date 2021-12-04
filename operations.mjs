import {
  makeGroupJoinPacket,
  makeGroupLeavePacket,
  makeScanPacket,
  readConnectPacket,
  readLoginPacket,
  scanCompletePacket,
} from "./packets.mjs";
import { connections, copyUsers } from "./users.mjs";
import { asyncWrite } from "./util.mjs";

const productCodeRe = /^[A-Z0-9]{9}$/;
const macRe = /^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i;

/**
 * @param {User} userState
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
  userState.loggedIn = true;
  userState.name = loginPacket.name;
  userState.game = loginPacket.game;
  userState.mac = loginPacket.mac.toUpperCase();
  return null;
}

const groupCodeRe = /^(?:[a-z]{,4}|[a-z]{4}\d{,4})$/i;

/**
 * @param {import("net").Socket} connection
 * @param {User} userState
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

  const group = connectPacket.group.toUpperCase();
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

/**
 * @param {string} userIp
 * @param {string} game
 * @param {string} group
 */
export async function leaveGroup(userIp, game, group) {
  const packet = makeGroupLeavePacket(userIp);
  for (const { 0: ip, 1: peer } of copyUsers()) {
    if (ip !== userIp && peer.game === game && peer.group === group) {
      await asyncWrite(connections.get(ip), packet);
    }
  }
}

/**
 * @param {import("net").Socket} connection
 * @param {User} userState
 * @returns {Promise<string|null>}
 */
export async function disconnect(connection, userState) {
  if (userState.group == null) {
    return "User is not in a group";
  }

  const { game, group } = userState;
  userState.group = null;
  await leaveGroup(connection.remoteAddress, game, group);
}

const added = (set, value) => set.size !== set.add(value).size;

const shouldSendScan = (userState, peer, game, seenGroups) =>
  peer !== userState && peer.game === game && added(seenGroups, peer.group);

/**
 * @param {import("net").Socket} connection
 * @param {User} userState
 * @returns {Promise<string|null>}
 */
export async function scan(connection, userState) {
  if (userState.group != null) {
    return `User scanned when in group ${userState.group} for game ${userState.game}`;
  }

  const users = copyUsers();
  const seenGroups = new Set();
  const { game } = userState;
  await Promise.all(users.flatMap(({ 1: peer }) => {
    return shouldSendScan(userState, peer, game, seenGroups)
      ? [asyncWrite(connection, makeScanPacket(peer))]
      : [];
  }));
  await asyncWrite(connection, scanCompletePacket);
}
