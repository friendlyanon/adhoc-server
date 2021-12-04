/**
 * @typedef {object} User
 * @property {boolean} loggedIn
 * @property {string|null} name
 * @property {string|null} game
 * @property {string|null} mac
 * @property {string|null} group
 */

/** @type {Map<string, import("net").Socket>} */
export const connections = new Map();

/** @returns {User} */
export const createUser = () => ({
  loggedIn: false,
  name: null,
  game: null,
  mac: null,
  group: null,
});

/** @typedef {Map<string, User>} Users */

/** @type {Users} */
export const users = new Map();

/** @typedef {{0: string, 1: User}} UserEntry */

/** @returns {UserEntry[]} */
export const copyUsers = () => Array.from(users.entries(), (entry) => {
  entry[1] = structuredClone(entry[1]);
  return entry;
});
