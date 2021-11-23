export const asyncWrite = (stream, data) => new Promise((resolve, reject) => {
  stream.write(data, (error) => {
    if (error != null) {
      reject(error);
    } else {
      resolve(null);
    }
  });
});
