/**
 * @param {number} x
 * @returns {string}
 */
const toHex = (x) => x.toString(16).padStart(2, "0");

/**
 * @param {Buffer} chunk
 * @returns {string}
 */
const readName = (chunk) => {
  const view = Buffer.from(chunk.buffer, 7, 128);
  const index = view.indexOf(0);
  const length = index === -1 ? 128 : index;
  return view.toString("utf8", 0, length);
};

/**
 * @param {Buffer} chunk
 */
export const readLoginPacket = (chunk) => ({
  mac: Array.from(chunk.slice(1, 7), toHex).join(":"),
  name: readName(chunk),
  game: chunk.toString("ascii", 135, 144),
});
