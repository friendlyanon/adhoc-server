export const asyncWrite = (stream, data) => new Promise((resolve, reject) => {
  stream.write(data, (error) => {
    if (error != null) {
      reject(error);
    } else {
      resolve(null);
    }
  });
});

/**
 * @param {string} ip
 * @returns {number}
 */
export const ipToUint32 = (ip) => {
  const [_1, _2, _3, _4] = ip.split(".").map(Number);
  return _1 << 24 | _2 << 16 | _3 << 8 | _4;
};
