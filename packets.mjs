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
