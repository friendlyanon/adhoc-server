import { MessageChannel } from "worker_threads";

/**
 * @template T
 * @param {T} value
 * @returns {Promise<T>}
 */
export function asyncStructuredClone(value) {
  const { port1, port2 } = new MessageChannel();
  return new Promise((resolve) => {
    port2.on("message", resolve);
    port2.on("close", () => port2.close);
    port1.postMessage(value);
    port1.close();
  });
}

export const asyncWrite = (stream, data) => new Promise((resolve, reject) => {
  stream.write(data, (error) => {
    if (error != null) {
      reject(error);
    } else {
      resolve(null);
    }
  });
});
