/**
 * @typedef {object} User
 */

/** @returns {User} */
export const createUser = () => ({});

/** @typedef {Map<string, User>} Users */

/** @type {Users} */
export const users = new Map();

/** @typedef {{0: string, 1: User}} UserEntry */

/** @returns {UserEntry[]} */
export const copyUsers = () =>
  structuredClone(Array.from(users.entries()));
