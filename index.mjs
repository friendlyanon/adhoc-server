import process from "process";
import { createInterface } from "readline";
import { http } from "./http.mjs";
import { tcp } from "./tcp.mjs";

const sigintSource = process.platform === "win32"
  ? createInterface({ input: process.stdin })
  : process;

const parseArgs = ([tcpPort, httpPort]) => [
  parseInt(tcpPort, 10) || 27312,
  parseInt(httpPort, 10) || 80,
];

const wrapError = (server) => new Promise((resolve) => {
  server.once("error", (error) => resolve(error));
});

const handleCtrlC = () => new Promise((resolve) => {
  sigintSource.once("SIGINT", () => resolve(null));
});

const listen = (server, name, port, signal) => new Promise((resolve) => {
  server.listen({ host: "0.0.0.0", port, signal }, () => {
    console.log("%s server listening on port %d", name, port);
    resolve(null);
  });
});

function main(args) {
  const [tcpPort, httpPort] = parseArgs(args);

  const httpServer = http();
  const tcpServer = tcp();

  const promise = Promise.race([
    wrapError(httpServer),
    wrapError(tcpServer),
    handleCtrlC(),
  ]);

  const controller = new AbortController();
  return Promise.all([
    listen(httpServer, "HTTP", httpPort, controller.signal),
    listen(tcpServer, "TCP", tcpPort, controller.signal),
  ]).then(() => promise.finally(() => {
    controller.abort();
    console.log("Aborted");
  }));
}

main(process.argv.slice(1)).then((error) => {
  if (error != null) {
    console.error(error);
    process.exit(1);
  } else {
    process.exit(0);
  }
});
