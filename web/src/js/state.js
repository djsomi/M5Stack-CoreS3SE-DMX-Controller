// module: state
// requires:
() => {
  // Stage 6B.2 will supply confirmed device state through this boundary.
  // Illustrative fader values live in the view, never in a Controller mirror.
  const foundation = Object.freeze({ready: false, connection: "Not connected", blackout: null});
  return {current: () => foundation};
}
