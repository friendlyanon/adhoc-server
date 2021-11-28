import * as opcodes from "./opcodes.mjs";
import { ipToUint32 } from "./util.mjs";

/**
 * @param {number} x
 * @returns {string}
 */
const toHex = (x) => x.toString(16).padStart(2, "0");

/**
 * @param {Buffer} chunk
 * @param {number} start
 * @param {number} length
 * @returns {string}
 */
const readName = (chunk, start, length) => {
  const view = Buffer.from(chunk.buffer, start, length);
  const index = view.indexOf(0);
  return view.toString("utf8", 0, index === -1 ? length : index);
};

/**
 * @param {Buffer} chunk
 */
export const readLoginPacket = (chunk) => ({
  mac: Array.from(chunk.slice(1, 7), toHex).join(":"),
  name: readName(chunk, 7, 128),
  game: chunk.toString("ascii", 135, 144),
});

/**
 * @param {Buffer} chunk
 */
export const readConnectPacket = (chunk) => ({
  group: readName(chunk, 1, 8),
});

/**
 * @param {Buffer} buffer
 * @param {string} mac
 * @param {number} offset
 */
const writeMac = (buffer, mac, offset) => {
  for (const [i, byte] of mac.split(":").entries()) {
    buffer.writeUInt8(parseInt(byte, 16), offset + i);
  }
};

/**
 * @param {UserEntry} entry
 * @returns {Buffer}
 */
export const makeGroupJoinPacket = (entry) => {
  const { 0: ip, 1: userState } = entry;
  const result = Buffer.alloc(1 + 128 + 6 + 4);
  result.writeUInt8(opcodes.CONNECT, 0);
  result.write(userState.name, 1, "ascii");
  writeMac(result, userState.mac, 129);
  result.writeUInt32BE(ipToUint32(ip), 135);
  return result;
};

/**
 * @param {string} ip
 * @returns {Buffer}
 */
export const makeGroupLeavePacket = (ip) => {
  const result = Buffer.allocUnsafe(5);
  result.writeUInt8(opcodes.DISCONNECT);
  result.writeUInt32BE(ipToUint32(ip), 1);
  return result;
};
