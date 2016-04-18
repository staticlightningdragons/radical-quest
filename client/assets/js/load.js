
var Load = {
    preload : function() {
        game.load.image('load', './assets/images/load.png');
        
        // align canvas
        game.scale.pageAlignHorizontally = true;
        game.scale.pageAlignVertically = true;
        var connection;
    },

    create : function() {
        // TODO change this to depend on player connecting.
        // that is, when another player is connected, it is
        // done "loading" and can go on to the next state
        this.add.sprite(0, 0, 'load'); // add background

        enterButton = game.input.keyboard.addKey(Phaser.Keyboard.ENTER);
        enterButton.onDown.add(this.check, this);
    },

    check : function() {
        this.state.start('Select');
    }
};
