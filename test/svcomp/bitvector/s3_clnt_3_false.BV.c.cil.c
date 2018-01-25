/* TAGS: sym big c */
/* VERIFY_OPTS: --symbolic --sequential -o ignore:control -o ignore:memory */
#include <stdbool.h>
#include <assert.h>
/* print_CIL_Input is true */
extern int __VERIFIER_nondet_int(void);
extern unsigned int __VERIFIER_nondet_uint(void);
extern long __VERIFIER_nondet_long(void);
extern char __VERIFIER_nondet_char(void);

int ssl3_connect(void)
{ int s__info_callback  = __VERIFIER_nondet_int();
  int s__in_handshake  = __VERIFIER_nondet_int();
  int s__state ;
  int s__new_session ;
  int s__server ;
  int s__version  = __VERIFIER_nondet_int();
  int s__type ;
  int s__init_num ;
  int s__bbio  = __VERIFIER_nondet_int();
  int s__wbio  = __VERIFIER_nondet_int();
  int s__hit  = __VERIFIER_nondet_int();
  int s__rwstate ;
  int s__init_buf___0 ;
  int s__debug  = __VERIFIER_nondet_int();
  int s__shutdown ;
  int s__ctx__info_callback  = __VERIFIER_nondet_int();
  int s__ctx__stats__sess_connect_renegotiate ;
  int s__ctx__stats__sess_connect ;
  int s__ctx__stats__sess_hit  = __VERIFIER_nondet_int();
  int s__ctx__stats__sess_connect_good  = __VERIFIER_nondet_int();
  int s__s3__change_cipher_spec ;
  int s__s3__flags ;
  int s__s3__delay_buf_pop_ret ;
  int s__s3__tmp__cert_req  = __VERIFIER_nondet_int();
  int s__s3__tmp__new_compression  = __VERIFIER_nondet_int();
  int s__s3__tmp__reuse_message  = __VERIFIER_nondet_int();
  int s__s3__tmp__new_cipher  = __VERIFIER_nondet_int();
  int s__s3__tmp__new_cipher__algorithms  = __VERIFIER_nondet_int();
  int s__s3__tmp__next_state___0 ;
  int s__s3__tmp__new_compression__id  = __VERIFIER_nondet_int();
  int s__session__cipher ;
  int s__session__compress_meth ;
  int buf ;
  unsigned long tmp ;
  unsigned long l ;
  int num1 ;
  int cb ;
  int ret ;
  int new_state ;
  int state ;
  int skip ;
  int tmp___0 ;
  int tmp___1  = __VERIFIER_nondet_int();
  int tmp___2  = __VERIFIER_nondet_int();
  int tmp___3  = __VERIFIER_nondet_int();
  int tmp___4  = __VERIFIER_nondet_int();
  int tmp___5  = __VERIFIER_nondet_int();
  int tmp___6  = __VERIFIER_nondet_int();
  int tmp___7  = __VERIFIER_nondet_int();
  int tmp___8  = __VERIFIER_nondet_int();
  int tmp___9  = __VERIFIER_nondet_int();
  int blastFlag ;
  int ag_X ;
  int ag_Y ;
  int ag_Z ;
  int __retres60 ;

  {
  s__state = 12292;
  blastFlag = 0;
  tmp = __VERIFIER_nondet_int();
  cb = 0;
  ret = -1;
  skip = 0;
  tmp___0 = 0;
  if (s__info_callback != 0) {
    cb = s__info_callback;
  } else {
    if (s__ctx__info_callback != 0) {
      cb = s__ctx__info_callback;
    } else {

    }
  }
  s__in_handshake = s__in_handshake + 1;
  if (tmp___1 + 12288) {
    if (tmp___2 + 16384) {

    } else {

    }
  } else {

  }
  if (s__hit) {
    ag_Y = 208;
  } else {
    ag_Z = 48;
  }
  {
  while (1) {
    while_0_continue: /* CIL Label */ ;
    state = s__state;
    if (s__state == 12292) {
      goto switch_1_12292;
    } else {
      if (s__state == 16384) {
        goto switch_1_16384;
      } else {
        if (s__state == 4096) {
          goto switch_1_4096;
        } else {
          if (s__state == 20480) {
            goto switch_1_20480;
          } else {
            if (s__state == 4099) {
              goto switch_1_4099;
            } else {
              if (s__state == 4368) {
                goto switch_1_4368;
              } else {
                if (s__state == 4369) {
                  goto switch_1_4369;
                } else {
                  if (s__state == 4384) {
                    goto switch_1_4384;
                  } else {
                    if (s__state == 4385) {
                      goto switch_1_4385;
                    } else {
                      if (s__state == 4400) {
                        goto switch_1_4400;
                      } else {
                        if (s__state == 4401) {
                          goto switch_1_4401;
                        } else {
                          if (s__state == 4416) {
                            goto switch_1_4416;
                          } else {
                            if (s__state == 4417) {
                              goto switch_1_4417;
                            } else {
                              if (s__state == 4432) {
                                goto switch_1_4432;
                              } else {
                                if (s__state == 4433) {
                                  goto switch_1_4433;
                                } else {
                                  if (s__state == 4448) {
                                    goto switch_1_4448;
                                  } else {
                                    if (s__state == 4449) {
                                      goto switch_1_4449;
                                    } else {
                                      if (s__state == 4464) {
                                        goto switch_1_4464;
                                      } else {
                                        if (s__state == 4465) {
                                          goto switch_1_4465;
                                        } else {
                                          if (s__state == 4466) {
                                            goto switch_1_4466;
                                          } else {
                                            if (s__state == 4467) {
                                              goto switch_1_4467;
                                            } else {
                                              if (s__state == 4480) {
                                                goto switch_1_4480;
                                              } else {
                                                if (s__state == 4481) {
                                                  goto switch_1_4481;
                                                } else {
                                                  if (s__state == 4496) {
                                                    goto switch_1_4496;
                                                  } else {
                                                    if (s__state == 4497) {
                                                      goto switch_1_4497;
                                                    } else {
                                                      if (s__state == 4512) {
                                                        goto switch_1_4512;
                                                      } else {
                                                        if (s__state == 4513) {
                                                          goto switch_1_4513;
                                                        } else {
                                                          if (s__state == 4528) {
                                                            goto switch_1_4528;
                                                          } else {
                                                            if (s__state == 4529) {
                                                              goto switch_1_4529;
                                                            } else {
                                                              if (s__state == 4560) {
                                                                goto switch_1_4560;
                                                              } else {
                                                                if (s__state == 4561) {
                                                                  goto switch_1_4561;
                                                                } else {
                                                                  if (s__state == 4352) {
                                                                    goto switch_1_4352;
                                                                  } else {
                                                                    if (s__state == 3) {
                                                                      goto switch_1_3;
                                                                    } else {
                                                                      {
                                                                      goto switch_1_default;
                                                                      if (0) {
                                                                        switch_1_12292: /* CIL Label */
                                                                        s__new_session = 1;
                                                                        s__state = 4096;
                                                                        s__ctx__stats__sess_connect_renegotiate = s__ctx__stats__sess_connect_renegotiate + 1;
                                                                        switch_1_16384: /* CIL Label */ ;
                                                                        switch_1_4096: /* CIL Label */ ;
                                                                        switch_1_20480: /* CIL Label */ ;
                                                                        switch_1_4099: /* CIL Label */
                                                                        s__server = 0;
                                                                        if (cb != 0) {

                                                                        } else {

                                                                        }
                                                                        if (s__version + 65280 != 768) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__type = 4096;
                                                                        if ((unsigned long )s__init_buf___0 == (unsigned long )((void *)0)) {
                                                                          buf = __VERIFIER_nondet_int();
                                                                          if ((unsigned long )buf == (unsigned long )((void *)0)) {
                                                                            ret = -1;
                                                                            goto end;
                                                                          } else {

                                                                          }
                                                                          if (! tmp___3) {
                                                                            ret = -1;
                                                                            goto end;
                                                                          } else {

                                                                          }
                                                                          s__init_buf___0 = buf;
                                                                        } else {

                                                                        }
                                                                        if (! tmp___4) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        if (! tmp___5) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4368;
                                                                        s__ctx__stats__sess_connect = s__ctx__stats__sess_connect + 1;
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4368: /* CIL Label */ ;
                                                                        switch_1_4369: /* CIL Label */
                                                                        s__shutdown = 0;
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (blastFlag == 0) {
                                                                          blastFlag = 1;
                                                                        } else {

                                                                        }
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4384;
                                                                        ag_X = s__state - 32;
                                                                        s__init_num = 0;
                                                                        if ((unsigned long )s__bbio != (unsigned long )s__wbio) {

                                                                        } else {

                                                                        }
                                                                        goto switch_1_break;
                                                                        switch_1_4384: /* CIL Label */ ;
                                                                        switch_1_4385: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (blastFlag == 1) {
                                                                          blastFlag = 2;
                                                                        } else {
                                                                          if (blastFlag == 4) {
                                                                            blastFlag = 5;
                                                                          } else {

                                                                          }
                                                                        }
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = ag_X;
                                                                        if (s__hit) {
                                                                          s__state = s__state | ag_Y;
                                                                        } else {
                                                                          s__state = s__state | ag_Z;
                                                                        }
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4400: /* CIL Label */ ;
                                                                        switch_1_4401: /* CIL Label */ ;
                                                                        if ((unsigned long )s__s3__tmp__new_cipher__algorithms + 256UL) {
                                                                          skip = 1;
                                                                        } else {
                                                                          ret = __VERIFIER_nondet_int();
                                                                          if (blastFlag == 2) {
                                                                            blastFlag = 3;
                                                                          } else {

                                                                          }
                                                                          if (ret <= 0) {
                                                                            goto end;
                                                                          } else {

                                                                          }
                                                                        }
                                                                        s__state = 4416;
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4416: /* CIL Label */ ;
                                                                        switch_1_4417: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (blastFlag == 3) {
                                                                          blastFlag = 4;
                                                                        } else {

                                                                        }
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4432;
                                                                        s__init_num = 0;
                                                                        if (! tmp___6) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        goto switch_1_break;
                                                                        switch_1_4432: /* CIL Label */ ;
                                                                        switch_1_4433: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (blastFlag <= 5) {
                                                                          goto ERROR;
                                                                        } else {

                                                                        }
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4448;
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4448: /* CIL Label */ ;
                                                                        switch_1_4449: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        if (s__s3__tmp__cert_req) {
                                                                          s__state = 4464;
                                                                        } else {
                                                                          s__state = 4480;
                                                                        }
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4464: /* CIL Label */ ;
                                                                        switch_1_4465: /* CIL Label */ ;
                                                                        switch_1_4466: /* CIL Label */ ;
                                                                        switch_1_4467: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4480;
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4480: /* CIL Label */ ;
                                                                        switch_1_4481: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        l = s__s3__tmp__new_cipher__algorithms;
                                                                        if (s__s3__tmp__cert_req == 1) {
                                                                          s__state = 4496;
                                                                        } else {
                                                                          s__state = 4512;
                                                                          s__s3__change_cipher_spec = 0;
                                                                        }
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4496: /* CIL Label */ ;
                                                                        switch_1_4497: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4512;
                                                                        s__init_num = 0;
                                                                        s__s3__change_cipher_spec = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4512: /* CIL Label */ ;
                                                                        switch_1_4513: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4528;
                                                                        s__init_num = 0;
                                                                        s__session__cipher = s__s3__tmp__new_cipher;
                                                                        if (s__s3__tmp__new_compression == 0) {
                                                                          s__session__compress_meth = 0;
                                                                        } else {
                                                                          s__session__compress_meth = s__s3__tmp__new_compression__id;
                                                                        }
                                                                        if (! tmp___7) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        if (! tmp___8) {
                                                                          ret = -1;
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        goto switch_1_break;
                                                                        switch_1_4528: /* CIL Label */ ;
                                                                        switch_1_4529: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        s__state = 4352;
                                                                        s__s3__flags = (long )s__s3__flags + -5L;
                                                                        if (s__hit) {
                                                                          s__s3__tmp__next_state___0 = 3;
                                                                          if ((long )s__s3__flags + 2L) {
                                                                            s__state = 3;
                                                                            s__s3__flags = (long )s__s3__flags * 4L;
                                                                            s__s3__delay_buf_pop_ret = 0;
                                                                          } else {

                                                                          }
                                                                        } else {
                                                                          s__s3__tmp__next_state___0 = 4560;
                                                                        }
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4560: /* CIL Label */ ;
                                                                        switch_1_4561: /* CIL Label */
                                                                        ret = __VERIFIER_nondet_int();
                                                                        if (ret <= 0) {
                                                                          goto end;
                                                                        } else {

                                                                        }
                                                                        if (s__hit) {
                                                                          s__state = 4512;
                                                                        } else {
                                                                          s__state = 3;
                                                                        }
                                                                        s__init_num = 0;
                                                                        goto switch_1_break;
                                                                        switch_1_4352: /* CIL Label */
                                                                        if ((long )num1 > 0L) {
                                                                          s__rwstate = 2;
                                                                          num1 = (long )tmp___9;
                                                                          if ((long )num1 <= 0L) {
                                                                            ret = -1;
                                                                            goto end;
                                                                          } else {

                                                                          }
                                                                          s__rwstate = 1;
                                                                        } else {

                                                                        }
                                                                        s__state = s__s3__tmp__next_state___0;
                                                                        goto switch_1_break;
                                                                        switch_1_3: /* CIL Label */
                                                                        if (s__init_buf___0 != 0) {
                                                                          s__init_buf___0 = 0;
                                                                        } else {

                                                                        }
                                                                        if (! ((long )s__s3__flags + 4L)) {

                                                                        } else {

                                                                        }
                                                                        s__init_num = 0;
                                                                        s__new_session = 0;
                                                                        if (s__hit) {
                                                                          s__ctx__stats__sess_hit = s__ctx__stats__sess_hit + 1;
                                                                        } else {

                                                                        }
                                                                        ret = 1;
                                                                        s__ctx__stats__sess_connect_good = s__ctx__stats__sess_connect_good + 1;
                                                                        if (cb != 0) {

                                                                        } else {

                                                                        }
                                                                        goto end;
                                                                        switch_1_default: /* CIL Label */
                                                                        ret = -1;
                                                                        goto end;
                                                                      } else {
                                                                        switch_1_break: /* CIL Label */ ;
                                                                      }
                                                                      }
                                                                    }
                                                                  }
                                                                }
                                                              }
                                                            }
                                                          }
                                                        }
                                                      }
                                                    }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    if (! s__s3__tmp__reuse_message) {
      if (! skip) {
        if (s__debug) {
          ret = __VERIFIER_nondet_int();
          if (ret <= 0) {
            goto end;
          } else {

          }
        } else {

        }
        if (cb != 0) {
          if (s__state != state) {
            new_state = s__state;
            s__state = state;
            s__state = new_state;
          } else {

          }
        } else {

        }
      } else {

      }
    } else {

    }
    skip = 0;
  }
  while_0_break: /* CIL Label */ ;
  }
  end:
  s__in_handshake = s__in_handshake - 1;
  if (cb != 0) {

  } else {

  }
  __retres60 = ret;
  goto return_label;
  ERROR: assert( 0 ); /* ERROR */
  {
  }
  __retres60 = -1;
  return_label: /* CIL Label */
  return (__retres60);
}
}
int main(void)
{ int __retres1 ;

  {
  {
  ssl3_connect();
  }
  __retres1 = 0;
  return (__retres1);
}
}
