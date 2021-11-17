import { asyncStructuredClone } from "./util.mjs";

/**
 * @typedef {object} User
 */

/** @returns {User} */
export const createUser = () => ({});

/** @typedef {Map<string, User>} Users */

/** @type {Users} */
export const users = new Map();

/** @typedef {{0: string, 1: User}} UserEntry */

/** @returns {Promise<UserEntry[]>} */
export const asyncCopyUsers = () =>
  asyncStructuredClone(Array.from(users.entries()));
