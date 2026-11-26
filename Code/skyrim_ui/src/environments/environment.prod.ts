import { Provider } from '@angular/core';

export const environment = {
  production: true,
  game: true,
  urlProtocol: 'https',
  url: 'skyrim-reborn-list.skyrim-together.com',
  githubUrl: 'https://api.github.com/repos/tiltedphoques/TiltedEvolution/tags',
  overwriteVersion: "",
  chatMessageLengthLimit: 512,

  providers: [] as Provider[],
};
