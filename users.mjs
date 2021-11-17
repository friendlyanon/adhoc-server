import { asyncStructuredClone } from "./util.mjs";

export const users = new Map();

export const asyncCopyUsers = () =>
  asyncStructuredClone(Array.from(users.entries()));
