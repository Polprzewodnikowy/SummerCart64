const showMenu = (event) => {
  event.currentTarget
    .classList
    .toggle('active');
  event.currentTarget
    .parentNode
    .parentNode
    .getElementsByTagName('menu')[0]
    .classList
    .toggle('mobile-hidden');
}
