/** Puts text on the clipboard; false when the copy command fails. */
export function copyText(text: string): boolean {
  // The overlay has no clipboard API; a selected text box and the copy command work
  const box = document.createElement('textarea');
  box.value = text;
  document.body.appendChild(box);
  box.select();
  const copied = document.execCommand('copy');
  box.remove();
  return copied;
}
