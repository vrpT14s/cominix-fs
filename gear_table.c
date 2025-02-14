#include "cominix.h"

u64 gear_table[256] = {
0xf11f8417de6594d7LL,
0x2450250d2e126a9bLL,
0x4f7a159051b3816aLL,
0x872fee8d090e7325LL,
0xa015ca0230210d6bLL,
0xd74615cfd76a37ceLL,
0xac0e47372f99aa4bLL,
0x69b1cc2b7cacf732LL,
0xcb1d4423f65a8454LL,
0x8f0f8479508c0135LL,
0x76b2b27eba5508deLL,
0x2cdcf43a5b8b284eLL,
0x52401bd0acba16bbLL,
0x2c370d9286eb187eLL,
0x3061a8caad0726afLL,
0x28dcfffa8928e8b7LL,
0xcd59b38e496c6a06LL,
0x4c533e31bfa3fb39LL,
0xf824a31b29426af6LL,
0x261a023df6141dfcLL,
0xa9f771d6dc85d3feLL,
0x28fd56e22af60333LL,
0x7f68c62635f078c3LL,
0xc8e3858432fb907bLL,
0xeb236106b75f235dLL,
0xec658bf05a8eac93LL,
0xce6fb6fc9388ce5eLL,
0x2499b30f4c655366LL,
0xdcd9863260c12218LL,
0xccf5070a5971b0abLL,
0x1ad510c0e7cecd40LL,
0xf9a10bbacfb7566eLL,
0xaf78a8ecd3d33438LL,
0x909b647e4c822227LL,
0x2005eddaaff80cc3LL,
0xf6fb8a7ad1ad8648LL,
0x9369a00e4350711fLL,
0x01112666f0adccaeLL,
0xa7e661e0b2880ee5LL,
0x333781e5df50556cLL,
0xfd5e82270103fdf6LL,
0xa804f8a6dc34ce8aLL,
0x54908a741806ae7dLL,
0xfbf73c2e7ddc594bLL,
0x075d24b6cd21aae8LL,
0x89cab34531dba16cLL,
0x0af2cfa24525a01bLL,
0x7d57f472cd729935LL,
0xa8569909fd9bc868LL,
0x5f33c258cbfce2d2LL,
0x33c32030d661819dLL,
0x5800401d923c9b84LL,
0xa0353f1169d1a2e5LL,
0xbd08400ad58dd5e0LL,
0x1bc6b725f23a12adLL,
0x724796d2acaaccb8LL,
0xe438339e1496b750LL,
0xf9286237d0a4262dLL,
0xed5cf03d60c2e010LL,
0x6edf5807b419fdf9LL,
0xc62df6b062966dd1LL,
0xee80aaeffbca80dcLL,
0xc2a8c5103e86b0e6LL,
0xa1ab19f8d9184c38LL,
0x30705d2b8ded4896LL,
0x4c2cc1b8bae48fe2LL,
0x81c0acf47d81e249LL,
0x3b59bda633c832a7LL,
0xe7aa73925c19687cLL,
0x93325bb1f7660182LL,
0xf2b4203b6ccc76a3LL,
0x0c1d8432d6e2982bLL,
0x1f535b240641ebc4LL,
0x58e452cc808196abLL,
0xca474357a681e871LL,
0xd087193119eee545LL,
0x7db81c7c94ce96e6LL,
0x4ae9ec7e7ae43704LL,
0x8cefdf3f3919804bLL,
0x14d8b9bb90ea7980LL,
0xfd828f63cfd9d111LL,
0x8d56271a886c146cLL,
0x0cf443a684e9e18bLL,
0x997fea35335590caLL,
0x8ab1205d80eb86eaLL,
0x4b27df0984623665LL,
0x96dceca39314c9b2LL,
0xa2fa4d5827093726LL,
0x3f574bf2793ac685LL,
0xb4895f3feb808c89LL,
0xca2a12d9dd9d0de7LL,
0xc76e84a83bcb5e53LL,
0x1c5f9d18a4269ea1LL,
0xed07f7802ecce574LL,
0xc300dd38b8ae11f3LL,
0x6e5096739dc0f173LL,
0xaed14801fd4c42feLL,
0x7b6f5c702e751110LL,
0x10eb06560a7318bdLL,
0x4a7192f03ccd970fLL,
0xbaab33acadbfb83aLL,
0x55c168fa74e4f71eLL,
0x31a62f42d9e4400aLL,
0x3b5bfe82d444beeaLL,
0xb775064fb961bc01LL,
0x069ba3c3d3fa4362LL,
0xfa612a3a95f7cb86LL,
0x839c57a223dddcd9LL,
0x1317cd0e6dc28ffdLL,
0x197d4d61fee13452LL,
0x2d327ceca3c6744bLL,
0x3f0733204b59fd3fLL,
0x4b0d7980a59f51a3LL,
0xbed4b57ff34db917LL,
0x40dd5ba459f33609LL,
0x670d7d44753dc3c9LL,
0xf75216e18fca2cf9LL,
0x1bc2c66d3b830cb9LL,
0x4c7d2f5e661c1432LL,
0x0608ca4516c95470LL,
0x323e0f750693d44aLL,
0xc0066dc521dcdc99LL,
0x5e2a27b51c8ef93eLL,
0xf4b2323db5e50a57LL,
0xc663ad2af619e1f9LL,
0x1ff62ac316a6ab04LL,
0x062d8255a086892bLL,
0x295fbb71fa7a0e88LL,
0x409e70501ab5159aLL,
0x6184439357fd0a3dLL,
0xee62751f4c154874LL,
0x0b70d1ac52fb782aLL,
0x563b20feca2ebab1LL,
0x8d44c539bd8ec0f6LL,
0x055e725ffc0df86bLL,
0x05c65b7636d3f4e2LL,
0xed259825cc0049dfLL,
0x1b06f91495c39ef5LL,
0xd9717eb2d74bd6e6LL,
0xb539ce8df48772f5LL,
0x4fcbbb8930e89e13LL,
0xc2b879a4b6374ea0LL,
0xc1cc3b96c5c3374cLL,
0xe20c5ec712983406LL,
0x970e4f3e1fc9db88LL,
0x2a51110241d9a94aLL,
0x3e61dcf71597ec94LL,
0xc0ee1d9b9687f8e8LL,
0x60eb956a27a3dcf6LL,
0xcd49d1c6483f3551LL,
0x825e74eb9d338ccbLL,
0x6e97592867a1f19fLL,
0x0012f175892540e9LL,
0xcadaaef86c6b43c5LL,
0x1bbf269225bb9405LL,
0x176228dcf44c9df1LL,
0x08b991cab0495247LL,
0x6c63d1236a8fda05LL,
0xf7760354d795208aLL,
0x9d99912b8b59fea0LL,
0x9eaf58157387fe63LL,
0x0d9459e2bb0532e6LL,
0xd8ed10d84a3097c3LL,
0xf48a34b73bd4fbd8LL,
0x90fb4036ba4ade49LL,
0x2715ed6680d9c2a1LL,
0x5de5ce9805d036faLL,
0x28942a9cd2b55e22LL,
0x7d46854be2a83a1aLL,
0x5d91415827f692b1LL,
0x17e775110b5fb245LL,
0x34f2b2b846c30de0LL,
0xf930cdc1af2af4b1LL,
0x6d61601a237d272fLL,
0xcec3025500658a81LL,
0x4a7bf1ea6e9c7eb2LL,
0x300b6ba89da9c391LL,
0x37195299499ace2fLL,
0x3178b716303cb537LL,
0xe63ff43c344335f7LL,
0x0501969155f0ee8fLL,
0x7daa875cb2d5db82LL,
0xdc000a8c519b03a3LL,
0x32dbc25cc773edc9LL,
0x639835c6549fb65aLL,
0x4220039de95138edLL,
0xe26d7f0e3bd74ae4LL,
0x768cbe20c1d761c6LL,
0xa8edacd9fd451393LL,
0x5c6e8b1b211a2f54LL,
0xe959a30e4a69c22bLL,
0xef64819e1fb12b3bLL,
0xb618b5e7f1af8698LL,
0xf7ce9f178952d5e8LL,
0x399c9aeccf5a3567LL,
0x74c8149afe714b9dLL,
0x85cfd2a5b178543bLL,
0x1ab6612987f84bafLL,
0xcf713db525d076aaLL,
0xe81d000324cd350dLL,
0x06a14b492bde2d76LL,
0x76ee99db92c7fe6cLL,
0x8f60d8bcb3a553baLL,
0x6f6beef7bcd2c267LL,
0x4d6d07e100a0e4f2LL,
0xed9c0c3a51858879LL,
0x2b829cd60fdc7502LL,
0x71fdd60db55aa624LL,
0x4521f8b39894c915LL,
0x61afb73ed4d9d9d7LL,
0xbe4a01bf50a5a389LL,
0xfca9e62f5df0bd91LL,
0xa7c0890d44fa05f7LL,
0x3f7b31ee0ed68b3cLL,
0xd6cc2ea3b22f1beeLL,
0x7488027360b9bf98LL,
0x7db436745ecb81e6LL,
0x9383901ea18e33beLL,
0x9b77be19a80cbc7eLL,
0x3a32c82b9c024943LL,
0xc331b5733ace8d7bLL,
0x7dc940aca5ade735LL,
0x8ab4261581021193LL,
0x903cbe13ad501387LL,
0x65c10d906ee67c86LL,
0x0cad9aa89ebf2005LL,
0x7bd8cc91e7ca76dfLL,
0xd9e21d036d14ecafLL,
0xe18edca9b6e84171LL,
0x3d90c5eea70db7d4LL,
0xa99cd9c89d446556LL,
0x1b365063bc732d53LL,
0xc7f80b7f98965851LL,
0x3fd6796c4cf283adLL,
0x59863e3066382899LL,
0x85cbe86bbe8644a5LL,
0x0f6be6cb529250cdLL,
0xde94f44e4f80fa11LL,
0xe6ba009c9ed8437eLL,
0xd85167bb0a915bbaLL,
0x8857469e28760924LL,
0xbe268143b5f4f3f5LL,
0x176f245560dec055LL,
0xa5112d4ce4427795LL,
0xd8d7a7d5d15f0d14LL,
0x04b249c29590b295LL,
0x193b523f27e71d5aLL,
0xf84ea1ad520dd735LL,
0xaf60d05a9198a907LL,
0xaf11a096d17a585dLL,
0x5cdfab7a588451edLL,
0x7e9c28b20770abb7LL,
0xaf259fefcc6d0c70LL,
0x5344ee46b8185fc3LL,
0x6b54d3a935ee37e2LL,
0x7620cbd50c249040LL,
};

